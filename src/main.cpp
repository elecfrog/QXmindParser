#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <unordered_set>

// Configuration namespace containing captured JSON fields
namespace cfg {
    // Set of JSON fields to be captured during processing
    static const std::unordered_set<QString> captured = {"rootTopic", "attached", "title", "children"};
}

// Forward declaration of internal utility functions
namespace {
    // read file from system
    std::optional<QString> readXmindJsonDoc(std::filesystem::path &&filePath);

    // get valid json document from the raw json string
    QString chopJsonDoc(QString &&rawJson);

    // process all nodes
    QString processNodes(QJsonDocument &&jsonDoc);

    QJsonValue processNodeRecursively(const QJsonValue &value);

    // entry of the code
    bool xmindParser(std::filesystem::path &&path);

    bool writeToDisk(QString &&str/*, const QString &filePath*/);
}

int main(int argc, char *argv[]) {

    if (argc < 2) { // check arguments
        qDebug() << "Usage:" << argv[0] << "<path to XMind file>";
        return -1;
    }

    if (std::filesystem::is_regular_file(argv[1])) {
        std::filesystem::path filePath = argv[1]; // get the file path from the arguments
        // Parse the specified XMind file
        if (xmindParser(std::move(filePath)))
            return 0;
    }

    return -1;
}


namespace {

    // Function to read the XMind file and extract its JSON content
    std::optional<QString> readXmindJsonDoc(std::filesystem::path &&filePath) {
        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Cannot open file for reading:" << file.errorString();
            return std::nullopt;
        }

        QTextStream in(&file);

        /*
         * The second line stores the json content of the XMind file.
         * */
        if (!in.atEnd()) in.readLine();  // Skip the first line
        if (!in.atEnd()) {
            QString ret = in.readLine();  // Read the second line
            file.close();

            // Clean the raw JSON content
            auto chopped = chopJsonDoc(std::move(ret));
            return chopped;
        }

        file.close();
        return std::nullopt;
    }

    // Function to clean the raw JSON content
    QString chopJsonDoc(QString &&rawJson) {
        // Remove any characters before "content.json"
        rawJson.replace(QRegularExpression("^.*content\\.json"), "");
        // Replace any characters after the JSON content
        rawJson.replace(QRegularExpression("}}]PK.+$"), "}}]");

        return rawJson;
    }

    // Function to process JSON nodes (either object or array)
    QString processNodes(QJsonDocument &&jsonDoc) {
        QJsonValue ret;

        if (jsonDoc.isObject()) {
            ret = processNodeRecursively(jsonDoc.object());
        } else if (jsonDoc.isArray()) {
            ret = processNodeRecursively(jsonDoc.array());
        }

        QJsonDocument doc;

        if (ret.isObject()) {
            doc.setObject(ret.toObject());
        } else if (ret.isArray()) {
            doc.setArray(ret.toArray());
        }

        return doc.toJson(QJsonDocument::Indented);
    }

    // Recursive function to process each JSON node
    QJsonValue processNodeRecursively(const QJsonValue &value) {
        if (value.isObject()) {
            QJsonObject obj = value.toObject();
            QJsonObject newObj;

            for (auto it = obj.begin(); it != obj.end(); ++it) {
                // Check if the current key is in the captured array
                if (cfg::captured.find(it.key()) != cfg::captured.end()) { // 1. Modify this line for set lookup
                    newObj.insert(it.key(), processNodeRecursively(it.value()));
                }
            }

            return newObj;

        } else if (value.isArray()) {
            QJsonArray array = value.toArray();
            QJsonArray newArray;

            for (const QJsonValue &item: array) {
                newArray.append(processNodeRecursively(item));  // Recursively process the array item
            }

            return newArray;

        } else {
            return value;  // Return the value directly if it's neither object nor array
        }
    }

    // Main function to parse the XMind file and print the processed JSON content
    bool xmindParser(std::filesystem::path &&path) {
        auto result = readXmindJsonDoc(std::move(path));

        if (result) {
            QString result_str = result.value();

            QJsonParseError error;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(result_str.toUtf8(), &error);

            if (error.error != QJsonParseError::NoError) {
                qDebug() << "Error parsing JSON:" << error.errorString();
                return false;
            }

            QString parsed = processNodes(std::move(jsonDoc));
            qDebug().noquote() << parsed;
            writeToDisk(std::move(parsed));

        } else {
            qDebug() << "Could not read the second line or the file does not have a second line.";
            return false;
        }

        return true;
    }

    bool writeToDisk(QString &&str/*, const QString &filePath*/) {
        QFile file("./parsed.json");
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "Cannot open file for writing:" << file.errorString();
            return false;
        }

        QTextStream out(&file);
        out << str;
        file.close();
        return true;
    }

}