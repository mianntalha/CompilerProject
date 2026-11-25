#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cctype>

using namespace std;

struct Token
{
    string type;
    string value;
    int line;
};

struct Variable
{
    string type;
};

struct Function
{
    string type;
    vector<string> parameters;
};

struct Structure
{
    map<string, string> members;
};

vector<Token> tokens;
size_t indexNumber = 0;
vector<map<string, Variable> > scopes;
map<string, Function> functions;
map<string, Structure> structures;
int errorCount = 0;
string currentFunctionType;

Token* current()
{
    if (indexNumber < tokens.size())
        return &tokens[indexNumber];

    return NULL;
}

bool hasValue(const string& value)
{
    Token* token = current();
    return token != NULL && token->value == value;
}

bool match(const string& value)
{
    if (!hasValue(value))
        return false;

    ++indexNumber;
    return true;
}

void startScope()
{
    map<string, Variable> newScope;
    scopes.push_back(newScope);
}

void endScope()
{
    if (!scopes.empty())
        scopes.pop_back();
}

void semanticError(int line, const string& message)
{
    cerr << "Semantic error on line " << line
         << ": " << message << ".\n";
    ++errorCount;
}

Variable* findVariable(const string& name)
{
    for (int i = (int)scopes.size() - 1; i >= 0; --i)
    {
        map<string, Variable>::iterator it = scopes[i].find(name);

        if (it != scopes[i].end())
            return &it->second;
    }

    return NULL;
}

bool readTokenFile(const string& path)
{
    ifstream input(path.c_str());

    if (!input.is_open())
    {
        cerr << "Error: cannot open token file '" << path << "'.\n";
        return false;
    }

    string line;

    while (getline(input, line))
    {
        size_t firstTab = line.find('\t');
        size_t secondTab = string::npos;

        if (firstTab != string::npos)
            secondTab = line.find('\t', firstTab + 1);

        if (firstTab == string::npos || secondTab == string::npos)
        {
            cerr << "Error: invalid token file format.\n";
            return false;
        }

        Token token;
        token.type = line.substr(0, firstTab);

        string lineText =
            line.substr(firstTab + 1, secondTab - firstTab - 1);

        token.line = 0;

        for (size_t i = 0; i < lineText.length(); ++i)
        {
            if (!isdigit((unsigned char)lineText[i]))
            {
                token.line = 0;
                break;
            }

            token.line = token.line * 10 + (lineText[i] - '0');
        }

        if (token.line < 1)
        {
            cerr << "Error: invalid line number in token file.\n";
            return false;
        }

        token.value = line.substr(secondTab + 1);

        string decoded;

        for (size_t i = 0; i < token.value.length(); ++i)
        {
            if (token.value[i] == '\\' &&
                i + 1 < token.value.length())
            {
                char next = token.value[++i];

                if (next == 'n')
                    decoded += '\n';
                else if (next == 't')
                    decoded += '\t';
                else if (next == 'r')
                    decoded += '\r';
                else
                    decoded += next;
            }
            else
            {
                decoded += token.value[i];
            }
        }

        token.value = decoded;
        tokens.push_back(token);
    }

    return true;
}

bool isIdentifierToken(Token* token)
{
    return token != NULL &&
           (token->type == "IDENTIFIER" ||
            token->value == "main");
}

string readType()
{
    if (match("int32"))
        return "int32";

    if (match("float32"))
        return "float32";

    if (match("struct"))
    {
        Token* name = current();

        if (name == NULL || !isIdentifierToken(name))
            return "error";

        ++indexNumber;
        return name->value;
    }

    return "error";
}

bool compatible(const string& first, const string& second)
{
    if (first == "error" || second == "error")
        return true;

    return first == second;
}

string expression();

vector<string> argumentList()
{
    vector<string> values;

    if (!hasValue(")"))
    {
        values.push_back(expression());

        while (match(","))
            values.push_back(expression());
    }

    match(")");
    return values;
}

string primaryExpression()
{
    Token* token = current();

    if (token == NULL)
        return "error";

    if (token->type == "INTEGER")
    {
        ++indexNumber;
        return "int32";
    }

    if (token->type == "FLOAT")
    {
        ++indexNumber;
        return "float32";
    }

    if (token->type == "STRING")
    {
        ++indexNumber;
        return "string";
    }

    if (isIdentifierToken(token))
    {
        string name = token->value;
        int line = token->line;
        ++indexNumber;

        if (match("("))
        {
            vector<string> arguments = argumentList();
            map<string, Function>::iterator function =
                functions.find(name);

            if (function == functions.end())
            {
                semanticError(line,
                    "function '" + name + "' is not declared");
                return "error";
            }

            if (arguments.size() !=
                function->second.parameters.size())
            {
                semanticError(line,
                    "wrong number of arguments for function '" +
                    name + "'");
                return function->second.type;
            }

            for (size_t i = 0;
                 i < arguments.size();
                 ++i)
            {
                if (!compatible(arguments[i],
                                 function->second.parameters[i]))
                {
                    semanticError(line,
                        "wrong argument type for function '" +
                        name + "'");
                    break;
                }
            }

            return function->second.type;
        }

        Variable* variable = findVariable(name);
        string variableType = "error";

        if (variable == NULL)
        {
            semanticError(line,
                "variable '" + name + "' is not declared");
        }
        else
        {
            variableType = variable->type;
        }

        while (match("."))
        {
            Token* member = current();

            if (member == NULL ||
                !isIdentifierToken(member))
                break;

            ++indexNumber;
            map<string, Structure>::iterator structure =
                structures.find(variableType);

            if (structure == structures.end())
            {
                semanticError(member->line,
                    "value is not a structure");
                variableType = "error";
            }
            else
            {
                map<string, string>::iterator field =
                    structure->second.members.find(member->value);

                if (field == structure->second.members.end())
                {
                    semanticError(member->line,
                        "structure has no member '" +
                        member->value + "'");
                    variableType = "error";
                }
                else
                {
                    variableType = field->second;
                }
            }
        }

        return variableType;
    }

    if (match("("))
    {
        string type = expression();

        if (match(")"))
            return type;

        return "error";
    }

    return "error";
}

string unaryExpression()
{
    if (match("!") || match("-") || match("+"))
        return unaryExpression();

    return primaryExpression();
}

string multiplicativeExpression()
{
    string type = unaryExpression();

    while (hasValue("*") ||
           hasValue("/") ||
           hasValue("%"))
    {
        ++indexNumber;
        string rightType = unaryExpression();

        if (type == "float32" || rightType == "float32")
            type = "float32";
        else
            type = "int32";
    }

    return type;
}

string additiveExpression()
{
    string type = multiplicativeExpression();

    while (hasValue("+") || hasValue("-"))
    {
        ++indexNumber;
        string rightType = multiplicativeExpression();

        if (type == "float32" || rightType == "float32")
            type = "float32";
        else
            type = "int32";
    }

    return type;
}

string relationalExpression()
{
    string type = additiveExpression();

    while (hasValue("<") ||
           hasValue(">") ||
           hasValue("<=") ||
           hasValue(">="))
    {
        ++indexNumber;
        additiveExpression();
        type = "int32";
    }

    return type;
}

string equalityExpression()
{
    string type = relationalExpression();

    while (hasValue("==") || hasValue("!="))
    {
        ++indexNumber;
        string rightType = relationalExpression();

        if (!compatible(type, rightType))
            type = "error";
        else
            type = "int32";
    }

    return type;
}

string andExpression()
{
    string type = equalityExpression();

    while (match("&&"))
    {
        equalityExpression();
        type = "int32";
    }

    return type;
}

string expression()
{
    string type = andExpression();

    while (match("||"))
    {
        andExpression();
        type = "int32";
    }

    return type;
}

void statement();

void block(bool newScope)
{
    if (!match("shru"))
        return;

    if (newScope)
        startScope();

    while (!hasValue("khatam") && current() != NULL)
        statement();

    match("khatam");

    if (newScope)
        endScope();
}

void variableDeclaration()
{
    string type = readType();
    Token* name = current();

    if (name == NULL || !isIdentifierToken(name))
        return;

    ++indexNumber;

    if (scopes.empty())
        startScope();

    map<string, Variable>::iterator old =
        scopes.back().find(name->value);

    if (old != scopes.back().end())
    {
        semanticError(name->line,
            "variable '" + name->value +
            "' is already declared");
    }
    else
    {
        Variable variable;
        variable.type = type;
        scopes.back()[name->value] = variable;
    }

    while (true)
    {
        if (match("=") || match("medalo"))
        {
            string expressionType = expression();

            if (!compatible(type, expressionType))
                semanticError(name->line,
                    "cannot assign " + expressionType +
                    " to " + type);
        }

        if (!match(","))
            break;

        name = current();

        if (name == NULL || !isIdentifierToken(name))
            break;

        ++indexNumber;

        old = scopes.back().find(name->value);

        if (old != scopes.back().end())
        {
            semanticError(name->line,
                "variable '" + name->value +
                "' is already declared");
        }
        else
        {
            Variable variable;
            variable.type = type;
            scopes.back()[name->value] = variable;
        }
    }

    match(";");
}

void structureDeclaration()
{
    match("struct");

    Token* name = current();

    if (name == NULL)
        return;

    ++indexNumber;
    match("shru");

    Structure structure;

    while (!hasValue("khatam") && current() != NULL)
    {
        string memberType = readType();
        Token* memberName = current();

        if (memberName == NULL ||
            !isIdentifierToken(memberName))
            break;

        ++indexNumber;

        if (structure.members.find(memberName->value) ==
            structure.members.end())
        {
            structure.members[memberName->value] =
                memberType;
        }
        else
        {
            semanticError(memberName->line,
                "structure member '" +
                memberName->value +
                "' is already declared");
        }

        match(";");
    }

    match("khatam");
    match(";");
    structures[name->value] = structure;
}

vector<string> parameterList(vector<string>& names)
{
    vector<string> types;

    if (!hasValue(")"))
    {
        types.push_back(readType());
        Token* name = current();

        if (name != NULL &&
            isIdentifierToken(name))
        {
            ++indexNumber;
            names.push_back(name->value);
        }

        while (match(","))
        {
            types.push_back(readType());
            name = current();

            if (name != NULL &&
                isIdentifierToken(name))
            {
                ++indexNumber;
                names.push_back(name->value);
            }
        }
    }

    match(")");
    return types;
}

void conditionalStatement()
{
    match("agar");
    match("(");
    expression();
    match(")");
    block(true);

    if (match("else"))
        block(true);
}

void loopStatement()
{
    match("jabtak");
    match("(");
    expression();
    match(")");
    block(true);
}

void inputStatement()
{
    match("parho");
    Token* name = current();

    if (name == NULL ||
        !isIdentifierToken(name))
        return;

    ++indexNumber;
    Variable* variable = findVariable(name->value);

    if (variable == NULL)
    {
        semanticError(name->line,
            "variable '" + name->value +
            "' is not declared");
    }

    while (match("."))
    {
        Token* member = current();

        if (member != NULL &&
            isIdentifierToken(member))
            ++indexNumber;
    }

    match(";");
}

void outputStatement()
{
    match("likho");
    expression();

    while (match(","))
        expression();

    match(";");
}

void identifierStatement()
{
    Token* name = current();

    if (name == NULL ||
        !isIdentifierToken(name))
        return;

    ++indexNumber;

    if (match("("))
    {
        vector<string> arguments =
            argumentList();

        map<string, Function>::iterator function =
            functions.find(name->value);

        if (function == functions.end())
        {
            semanticError(name->line,
                "function '" + name->value +
                "' is not declared");
        }
        else if (arguments.size() !=
                 function->second.parameters.size())
        {
            semanticError(name->line,
                "wrong number of arguments for function '" +
                name->value + "'");
        }

        match(";");
        return;
    }

    Variable* variable = findVariable(name->value);
    string variableType = "error";

    if (variable == NULL)
    {
        semanticError(name->line,
            "variable '" + name->value +
            "' is not declared");
    }
    else
    {
        variableType = variable->type;
    }

    while (match("."))
    {
        Token* member = current();

        if (member == NULL ||
            !isIdentifierToken(member))
            break;

        ++indexNumber;
        map<string, Structure>::iterator structure =
            structures.find(variableType);

        if (structure == structures.end())
        {
            semanticError(member->line,
                "value is not a structure");
            variableType = "error";
        }
        else
        {
            map<string, string>::iterator field =
                structure->second.members.find(member->value);

            if (field == structure->second.members.end())
            {
                semanticError(member->line,
                    "structure has no member '" +
                    member->value + "'");
                variableType = "error";
            }
            else
            {
                variableType = field->second;
            }
        }
    }

    if (!match("medalo") && !match("="))
        return;

    string expressionType = expression();

    if (!compatible(variableType, expressionType))
        semanticError(name->line,
            "cannot assign " + expressionType +
            " to " + variableType);

    match(";");
}

void statement()
{
    Token* token = current();

    if (token == NULL)
        return;

    if (hasValue("int32") ||
        hasValue("float32") ||
        hasValue("struct"))
    {
        variableDeclaration();
        return;
    }

    if (hasValue("agar"))
    {
        conditionalStatement();
        return;
    }

    if (hasValue("jabtak"))
    {
        loopStatement();
        return;
    }

    if (hasValue("likho"))
    {
        outputStatement();
        return;
    }

    if (hasValue("parho"))
    {
        inputStatement();
        return;
    }

    if (match("return"))
    {
        int line = token->line;
        string expressionType = expression();

        if (currentFunctionType != "" &&
            !compatible(currentFunctionType,
                        expressionType))
        {
            semanticError(line,
                "cannot return " + expressionType +
                " from function returning " +
                currentFunctionType);
        }

        match(";");
        return;
    }

    if (hasValue("shru"))
    {
        block(true);
        return;
    }

    if (isIdentifierToken(token))
    {
        identifierStatement();
        return;
    }

    ++indexNumber;
}

void globalDeclaration()
{
    string type = readType();
    Token* name = current();

    if (name == NULL ||
        !isIdentifierToken(name))
        return;

    ++indexNumber;

    if (match("("))
    {
        vector<string> parameterNames;
        vector<string> parameterTypes =
            parameterList(parameterNames);

        if (functions.find(name->value) ==
            functions.end())
        {
            Function function;
            function.type = type;
            function.parameters = parameterTypes;
            functions[name->value] = function;
        }
        else
        {
            semanticError(name->line,
                "function '" + name->value +
                "' is already declared");
        }

        if (match(";"))
            return;

        startScope();

        for (size_t i = 0;
             i < parameterNames.size();
             ++i)
        {
            Variable variable;
            variable.type = parameterTypes[i];
            scopes.back()[parameterNames[i]] = variable;
        }

        string oldFunctionType = currentFunctionType;
        currentFunctionType = type;
        block(false);
        currentFunctionType = oldFunctionType;
        endScope();
        return;
    }

    if (scopes.empty())
        startScope();

    if (scopes[0].find(name->value) ==
        scopes[0].end())
    {
        Variable variable;
        variable.type = type;
        scopes[0][name->value] = variable;
    }
    else
    {
        semanticError(name->line,
            "global variable '" + name->value +
            "' is already declared");
    }

    while (true)
    {
        if (match("=") || match("medalo"))
        {
            string expressionType = expression();

            if (!compatible(type, expressionType))
                semanticError(name->line,
                    "cannot assign " + expressionType +
                    " to " + type);
        }

        if (!match(","))
            break;

        name = current();

        if (name == NULL ||
            !isIdentifierToken(name))
            break;

        ++indexNumber;

        if (scopes[0].find(name->value) ==
            scopes[0].end())
        {
            Variable variable;
            variable.type = type;
            scopes[0][name->value] = variable;
        }
        else
        {
            semanticError(name->line,
                "global variable '" + name->value +
                "' is already declared");
        }
    }

    match(";");
}

void program()
{
    startScope();

    while (current() != NULL)
    {
        if (hasValue("struct"))
        {
            structureDeclaration();
        }
        else if (hasValue("int32") ||
                 hasValue("float32"))
        {
            globalDeclaration();
        }
        else
        {
            ++indexNumber;
        }
    }

    map<string, Function>::iterator mainFunction =
        functions.find("main");

    if (mainFunction == functions.end())
    {
        semanticError(1,
            "main function is not declared");
    }
    else if (!mainFunction->second.parameters.empty())
    {
        semanticError(1,
            "main function cannot have parameters");
    }
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: " << argv[0]
             << " <tokens.txt>\n";
        return 1;
    }

    if (!readTokenFile(argv[1]))
        return 1;

    program();

    if (errorCount != 0)
    {
        cerr << errorCount
             << " semantic error(s) found.\n";
        return 1;
    }

    cout << "Semantics are correct.\n";
    return 0;
}
