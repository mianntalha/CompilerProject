
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <iterator>

using namespace std;

struct Token
{
    string category;
    string value;
    int row;
};

// Check whether a word belongs to the language's reserved words
bool checkKeyword(const string& value)
{
    const string keywords[] = {
        "int32", "float32", "struct", "agar",
        "else", "jabtak", "shru", "khatam",
        "likho", "parho", "return", "main"
    };

    for (const string& keyword : keywords)
    {
        if (value == keyword)
            return true;
    }le

    return false;
}

// Prepare special characters before saving tokens to a file
string formatLexeme(const string& value)
{
    string formatted;

    for (char ch : value)
    {
        switch (ch)
        {
            case '\\': formatted += "\\\\"; break;
            case '\t': formatted += "\\t"; break;
            case '\n': formatted += "\\n"; break;
            case '\r': formatted += "\\r"; break;
            default: formatted += ch; break;
        }
    }

    return formatted;
}

int main(int argc, char* argv[])
{
    if (argc != 2 && argc != 3)
    {
        cerr << "Usage: " << argv[0]
             << " <source.citu> [tokens.txt]\n";
        return 1;
    }

    string inputFile = argv[1];
    string outputFile = (argc == 3) ? argv[2] : "tokens.txt";

    ifstream file(inputFile.c_str());

    if (!file.is_open())
    {
        cerr << "Error: unable to open source file '"
             << inputFile << "'.\n";
        return 1;
    }

    string sourceCode(
        (istreambuf_iterator<char>(file)),
        istreambuf_iterator<char>()
    );

    vector<Token> tokenStream;

    size_t position = 0;
    int currentLine = 1;
    int errorCount = 0;

    while (position < sourceCode.length())
    {
        unsigned char ch =
            static_cast<unsigned char>(sourceCode[position]);

        // Ignore whitespace and maintain line numbers
        if (isspace(ch))
        {
            if (sourceCode[position] == '\n')
                ++currentLine;

            ++position;
            continue;
        }

        // Identify keywords and variable names
        if (isalpha(ch) || sourceCode[position] == '_')
        {
            size_t beginning = position;
            int startLine = currentLine;

            while (position < sourceCode.length())
            {
                unsigned char next =
                    static_cast<unsigned char>(sourceCode[position]);

                if (!isalnum(next) && sourceCode[position] != '_')
                    break;

                ++position;
            }

            string identifier = sourceCode.substr(
                beginning, position - beginning
            );

            string tokenCategory;

            if (identifier == "medalo")
                tokenCategory = "OPERATOR";
            else if (checkKeyword(identifier))
                tokenCategory = "KEYWORD";
            else
                tokenCategory = "IDENTIFIER";

            tokenStream.push_back({
                tokenCategory, identifier, startLine
            });

            continue;
        }

        // Recognize integer and floating-point values
        if (isdigit(ch))
        {
            size_t beginning = position;
            int startLine = currentLine;

            while (position < sourceCode.length() &&
                   isdigit(static_cast<unsigned char>(
                       sourceCode[position])))
            {
                ++position;
            }

            string tokenCategory = "INTEGER";

            if (position + 1 < sourceCode.length() &&
                sourceCode[position] == '.' &&
                isdigit(static_cast<unsigned char>(
                    sourceCode[position + 1])))
            {
                tokenCategory = "FLOAT";
                ++position;

                while (position < sourceCode.length() &&
                       isdigit(static_cast<unsigned char>(
                           sourceCode[position])))
                {
                    ++position;
                }
            }

            tokenStream.push_back({
                tokenCategory,
                sourceCode.substr(beginning, position - beginning),
                startLine
            });

            continue;
        }

        // Handle string literals
        if (sourceCode[position] == '"')
        {
            int startLine = currentLine;
            string literal(1, sourceCode[position]);
            ++position;

            bool stringClosed = false;

            while (position < sourceCode.length())
            {
                char next = sourceCode[position++];
                literal += next;

                if (next == '\n')
                    ++currentLine;

                if (next == '\\' && position < sourceCode.length())
                {
                    literal += sourceCode[position];

                    if (sourceCode[position] == '\n')
                        ++currentLine;

                    ++position;
                }
                else if (next == '"')
                {
                    stringClosed = true;
                    break;
                }
                else if (next == '\n')
                {
                    break;
                }
            }

            if (stringClosed)
            {
                tokenStream.push_back({
                    "STRING", literal, startLine
                });
            }
            else
            {
                cerr << "Lexical error on line " << startLine
                     << ": unterminated string literal.\n";

                ++errorCount;
            }

            continue;
        }

        // Check operators containing two characters
        int startLine = currentLine;
        string pair;

        if (position + 1 < sourceCode.length())
            pair = sourceCode.substr(position, 2);

        if (pair == "==" || pair == "!=" ||
            pair == "<=" || pair == ">=" ||
            pair == "&&" || pair == "||")
        {
            tokenStream.push_back({
                "OPERATOR", pair, startLine
            });

            position += 2;
            continue;
        }

        // Single-character operators
        string operators = "+-*/%=<>!";

        if (operators.find(sourceCode[position]) != string::npos)
        {
            tokenStream.push_back({
                "OPERATOR",
                string(1, sourceCode[position]),
                startLine
            });

            ++position;
            continue;
        }

        // Punctuation and grouping symbols
        string symbols = ";,(){}.";

        if (symbols.find(sourceCode[position]) != string::npos)
        {
            tokenStream.push_back({
                "PUNCTUATION",
                string(1, sourceCode[position]),
                startLine
            });

            ++position;
            continue;
        }

        // Report characters that are not supported
        cerr << "Lexical error on line " << currentLine
             << ": invalid character '"
             << sourceCode[position] << "'.\n";

        ++errorCount;
        ++position;
    }

    // Do not generate a token file if lexical errors exist
    if (errorCount != 0)
    {
        cerr << errorCount
             << " lexical error(s); token file was not written.\n";

        return 1;
    }

    // Save the generated token stream
    ofstream resultFile(outputFile.c_str());

    if (!resultFile.is_open())
    {
        cerr << "Error: unable to create token file '"
             << outputFile << "'.\n";

        return 1;
    }

    cout << "Tokens (type, line, lexeme):\n";

    for (const Token& item : tokenStream)
    {
        cout << "[" << item.category << "] line "
             << item.row << ": " << item.value << "\n";

        resultFile << item.category << '\t'
                   << item.row << '\t'
                   << formatLexeme(item.value) << '\n';
    }

    if (!resultFile)
    {
        cerr << "Error: could not finish writing token file '"
             << outputFile << "'.\n";

        return 1;
    }

    cout << "Token file written to " << outputFile << ".\n";

    return 0;
}

