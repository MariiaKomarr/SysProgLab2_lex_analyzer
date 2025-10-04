// .\jlexer
#include <bits/stdc++.h>
using namespace std;

enum class TokType {
    WHITESPACE,
    COMMENT,
    PREPROCESSOR,        // для рядків, що починаються з '#', хоча у Java не використовується
    KEYWORD,
    IDENTIFIER,
    NUMBER,
    STRING,
    CHAR,
    ANNOTATION,
    OPERATOR,
    DELIMITER,
    BOOLEAN_NULL,
    ERROR_TOKEN,
    EOF_TOKEN
};

struct Token {
    TokType type;
    string  lexeme;
    size_t  line, col;
    string  err;
};

struct LexerOptions {
    bool color = false;
};

struct Lexer {
    string src;
    size_t i = 0, n = 0;
    size_t line = 1, col = 1;
    LexerOptions opt;

    unordered_set<string> keywords{
        "abstract","assert","boolean","break","byte","case","catch","char","class","const",
        "continue","default","do","double","else","enum","extends","final","finally","float",
        "for","goto","if","implements","import","instanceof","int","interface","long","native",
        "new","package","private","protected","public","return","short","static","strictfp",
        "super","switch","synchronized","this","throw","throws","transient","try","void",
        "volatile","while","var","record","sealed","permits","non-sealed","yield",
        "module","open","opens","requires","exports","uses","provides","to","with","transitive"
    };
    unordered_set<string> booleanNull{ "true","false","null" };

    Lexer(string s, const LexerOptions& o): src(move(s)), n(src.size()), opt(o) {}

    char peek(int k=0) const { return (i+k<n)? src[i+k] : '\0'; }
    bool eof() const { return i>=n; }

    char get() {
        if (eof()) return '\0';
        char c = src[i++];
        if (c=='\n'){ line++; col=1; } else col++;
        return c;
    }
    void unget() {
        if (i==0) return;
        i--;
        if (src[i]=='\n'){
            // приблизна
            line = max<size_t>(1, line-1);
            col = 1; // колону при unget після \n залишаємо 1 для спрощення
        } else {
            col = max<size_t>(1, col-1);
        }
    }

    static bool isIdStart(char c){
        return (c=='_' || c=='$' || isalpha((unsigned char)c));
    }
    static bool isIdPart(char c){
        return (c=='_' || c=='$' || isalnum((unsigned char)c));
    }
    static bool isHex(char c){
        return isdigit((unsigned char)c) || (c>='a'&&c<='f') || (c>='A'&&c<='F');
    }
    static bool isBin(char c){ return c=='0' || c=='1'; }
    static bool isOct(char c){ return c>='0' && c<='7'; }

    Token make(TokType t, const string& s, size_t L, size_t C, string err=""){
        return Token{t,s,L,C,err};
    }

    // Пропуск пробілів, зберігаємо для можливого кольорового режиму
    Token lexWhitespace(){
        size_t L=line, C=col;
        string s;
        while(!eof()){
            char c=peek();
            if (c==' '||c=='\t'||c=='\r'||c=='\n'){ s.push_back(get()); }
            else break;
        }
        return make(TokType::WHITESPACE, s, L, C);
    }

    // Коментарі: // ... \n  або  /* ... */
    Token lexComment(){
        size_t L=line, C=col;
        string s;
        char c1=get(); s.push_back(c1); // '/'
        char c2=peek();
        if (c2=='/'){
            s.push_back(get());
            while(!eof()){
                char c= get();
                s.push_back(c);
                if (c=='\n') break;
            }
            return make(TokType::COMMENT, s, L, C);
        } else if (c2=='*'){
            s.push_back(get());
            bool closed=false;
            while(!eof()){
                char c=get(); s.push_back(c);
                if (c=='*' && peek()=='/'){ s.push_back(get()); closed=true; break; }
            }
            if (!closed) return make(TokType::ERROR_TOKEN, s, L, C, "Unterminated block comment");
            return make(TokType::COMMENT, s, L, C);
        } else {
            // це був просто оператор '/'
            unget();
            return lexOperatorOrDelimiter();
        }
    }

    // Рядок "..."
    Token lexString(){
        size_t L=line, C=col;
        string s;
        s.push_back(get()); // opening "
        bool closed=false;
        while(!eof()){
            char c=get(); s.push_back(c);
            if (c=='\\'){ // escape
                if (!eof()){ s.push_back(get()); }
                else break;
            } else if (c=='"'){ closed=true; break; }
            else if (c=='\n'){ return make(TokType::ERROR_TOKEN, s, L, C, "Unterminated string literal"); }
        }
        if (!closed) return make(TokType::ERROR_TOKEN, s, L, C, "Unterminated string literal");
        return make(TokType::STRING, s, L, C);
    }

    // Символьний літерал 'a' або '\n' або '\uXXXX'
    Token lexChar(){
        size_t L=line, C=col;
        string s;
        s.push_back(get()); // opening '
        bool closed=false;
        while(!eof()){
            char c=get(); s.push_back(c);
            if (c=='\\'){ // escape
                if (!eof()){ s.push_back(get()); }
                else break;
            } else if (c=='\''){ closed=true; break; }
            else if (c=='\n'){ return make(TokType::ERROR_TOKEN, s, L, C, "Unterminated char literal"); }
        }
        if (!closed) return make(TokType::ERROR_TOKEN, s, L, C, "Unterminated char literal");
        // мінімальна перевірка вмісту пропущена для простоти
        return make(TokType::CHAR, s, L, C);
    }

    // Числа: десяткові/плаваючі/hex/bin/octal, з підкресленнями та суфіксами L/l/F/f/D/d
    Token lexNumber(){
        size_t L=line, C=col;
        string s;
        auto push=[&](char c){ s.push_back(c); get(); };

        char c=peek();
        if (c=='0'){
            // 0x..., 0b..., 0..., або 0 сам
            push(c);
            char n1=peek();
            if (n1=='x' || n1=='X'){
                push(n1);
                if (!isHex(peek())) return make(TokType::ERROR_TOKEN, s, L, C, "Invalid hex literal");
                while(isHex(peek()) || peek()=='_'){ push(peek()); }
            } else if (n1=='b' || n1=='B'){
                push(n1);
                if (!isBin(peek())) return make(TokType::ERROR_TOKEN, s, L, C, "Invalid binary literal");
                while(isBin(peek()) || peek()=='_'){ push(peek()); }
            } else if (isdigit((unsigned char)n1) || n1=='_'){ // octal
                while(isOct(peek()) || peek()=='_'){ push(peek()); }
            }
            // суфікс цілого
            if (peek()=='l' || peek()=='L'){ push(peek()); }
            return make(TokType::NUMBER, s, L, C);
        }

        // Десяткове/плаваюче
        bool seenDot=false, seenExp=false;
        while(isdigit((unsigned char)peek()) || peek()=='_' ){ push(peek()); }
        if (peek()=='.'){
            // дивимося, чи це не діапазон оператор (наприклад, "...") — трактуємо як число з крапкою
            seenDot=true; push('.');
            while(isdigit((unsigned char)peek()) || peek()=='_'){ push(peek()); }
        }
        if (peek()=='e' || peek()=='E'){
            seenExp=true; push(peek());
            if (peek()=='+' || peek()=='-') push(peek());
            if (!isdigit((unsigned char)peek())) return make(TokType::ERROR_TOKEN, s, L, C, "Invalid exponent");
            while(isdigit((unsigned char)peek()) || peek()=='_'){ push(peek()); }
        }
        // суфікс типу
        if (peek()=='f'||peek()=='F'||peek()=='d'||peek()=='D'||peek()=='l'||peek()=='L'){ push(peek()); }

        if (!s.empty() && s.back()=='_'){
            return make(TokType::ERROR_TOKEN, s, L, C, "Numeric literal cannot end with underscore");
        }
        return make(TokType::NUMBER, s, L, C);
    }

    // Ідентифікатори/ключові/true/false/null
    Token lexIdentifierOrKeyword(){
        size_t L=line, C=col;
        string s;
        s.push_back(get());
        while(isIdPart(peek())) s.push_back(get());

        if (booleanNull.count(s)) return make(TokType::BOOLEAN_NULL, s, L, C);
        if (keywords.count(s))    return make(TokType::KEYWORD, s, L, C);
        return make(TokType::IDENTIFIER, s, L, C);
    }

    // Анотації
    Token lexAnnotation(){
        size_t L=line, C=col;
        string s;
        s.push_back(get()); // '@'
        if (!isIdStart(peek())){
            return make(TokType::ERROR_TOKEN, s, L, C, "Invalid annotation start");
        }
        s.push_back(get());
        while(isIdPart(peek())) s.push_back(get());
        // імена типу @org.example.Name
        if (peek()=='.'){
            while(peek()=='.'){
                s.push_back(get());
                if (!isIdStart(peek())) return make(TokType::ERROR_TOKEN, s, L, C, "Invalid qualified annotation");
                s.push_back(get());
                while(isIdPart(peek())) s.push_back(get());
            }
        }
        return make(TokType::ANNOTATION, s, L, C);
    }

    // Оператори та розділові знаки
    Token lexOperatorOrDelimiter(){
        size_t L=line, C=col;
        string s;
        char c = get();
        auto peek2 = [&]{ return string()+ (peek()?peek():'\0') + ((i+1<n)?src[i+1]:'\0'); };
        auto peek3 = [&]{ return string()+ (peek()?peek():'\0') + ((i+1<n)?src[i+1]:'\0') + ((i+2<n)?src[i+2]:'\0'); };

        // Розділові одинаки
        string delims = "(){},;[].";
        if (delims.find(c)!=string::npos){
            s.push_back(c);
            return make(TokType::DELIMITER, s, L, C);
        }

        // Потенційні багатосимвольні оператори
        s.push_back(c);
        string two = peek2();
        string three = peek3();

        // Порядок від найдовших
        // Трьохсимвольні
        if (s=="<" && three.substr(0,2)=="<=" && peek3()[2]=='='){ /* not a valid pattern */ }
        // >>>=  >>>  <<=  <<=
        if (c=='>' && three==">>="){ s+=get(); s+=get(); s+=get(); return make(TokType::OPERATOR,s,L,C);}
        if (c=='>' && three==">>>"){ s+=get(); s+=get(); return make(TokType::OPERATOR,s,L,C);}
        if (c=='<' && three=="<<="){ s+=get(); s+=get(); s+=get(); return make(TokType::OPERATOR,s,L,C);}
        // Двосимвольні
        if ( (c=='+'&&peek()=='+') || (c=='-'&&peek()=='-') ||
             (c=='+'&&peek()=='=') || (c=='-'&&peek()=='=') || (c=='*'&&peek()=='=') ||
             (c=='/'&&peek()=='=') || (c=='%'&&peek()=='=') || (c=='&'&&peek()=='=') ||
             (c=='|'&&peek()=='=') || (c=='^'&&peek()=='=') || (c=='<'&&peek()=='<') ||
             (c=='>'&&peek()=='>') || (c=='='&&peek()=='=') || (c=='!'&&peek()=='=') ||
             (c=='>'&&peek()=='=') || (c=='<'&&peek()=='=') || (c==':'&&peek()==':') ||
             (c=='-'&&peek()=='>')
           ){
            s.push_back(get());
            if (s==">>" && peek()=='='){ s.push_back(get()); }
            return make(TokType::OPERATOR, s, L, C);
        }

        // Односимвольні оператори
        string singleOps = "+-*/%&|^!~?:=<>";
        if (singleOps.find(c)!=string::npos){
            return make(TokType::OPERATOR, s, L, C);
        }

        // Нерозпізнаний символ
        return make(TokType::ERROR_TOKEN, s, L, C, "Unrecognized character");
    }

    // Лінійні препроцесорні директиви (не Java)
    Token lexPreprocessor(){
        size_t L=line, C=col;
        string s;
        s.push_back(get()); // '#'
        while(!eof() && peek()!='\n'){ s.push_back(get()); }
        return make(TokType::PREPROCESSOR, s, L, C);
    }

    Token next() {
        if (eof()) return make(TokType::EOF_TOKEN, "", line, col);
        char c = peek();

        // Пробіли/переноси
        if (c==' '||c=='\t'||c=='\r'||c=='\n') return lexWhitespace();

        // Коментарі або оператор '/'
        if (c=='/') return lexComment();

        // Рядки/символи
        if (c=='"') return lexString();
        if (c=='\'') return lexChar();

        // Препроцесор
        if (c=='#') return lexPreprocessor();

        // Анотація
        if (c=='@') return lexAnnotation();

        // Числа
        if (isdigit((unsigned char)c)) return lexNumber();
        // Крапка як початок числа типу .5
        if (c=='.' && isdigit((unsigned char)peek(1))){
            return lexNumber();
        }

        // Ідентифікатор/зарезервоване
        if (isIdStart(c)) return lexIdentifierOrKeyword();

        // Оператори/розділові
        return lexOperatorOrDelimiter();
    }
};

//Кольори
string colorFor(TokType t){
    switch(t){
        case TokType::KEYWORD:       return "\033[95m"; // magenta
        case TokType::BOOLEAN_NULL:  return "\033[95m";
        case TokType::IDENTIFIER:    return "\033[94m"; // blue
        case TokType::NUMBER:        return "\033[96m"; // cyan
        case TokType::STRING:        return "\033[92m"; // green
        case TokType::CHAR:          return "\033[92m";
        case TokType::COMMENT:       return "\033[90m"; // gray
        case TokType::OPERATOR:      return "\033[33m"; // yellow
        case TokType::DELIMITER:     return "\033[33m";
        case TokType::ANNOTATION:    return "\033[93m"; // bright yellow
        case TokType::PREPROCESSOR:  return "\033[91m"; // red (бо не Java)
        case TokType::ERROR_TOKEN:   return "\033[41;37m"; // white on red bg
        default: return "\033[0m";
    }
}

string typeName(TokType t){
    switch(t){
        case TokType::WHITESPACE:   return "WHITESPACE";
        case TokType::COMMENT:      return "COMMENT";
        case TokType::PREPROCESSOR: return "PREPROCESSOR";
        case TokType::KEYWORD:      return "KEYWORD";
        case TokType::IDENTIFIER:   return "IDENTIFIER";
        case TokType::NUMBER:       return "NUMBER";
        case TokType::STRING:       return "STRING";
        case TokType::CHAR:         return "CHAR";
        case TokType::ANNOTATION:   return "ANNOTATION";
        case TokType::OPERATOR:     return "OPERATOR";
        case TokType::DELIMITER:    return "DELIMITER";
        case TokType::BOOLEAN_NULL: return "BOOLEAN_NULL";
        case TokType::ERROR_TOKEN:  return "ERROR";
        case TokType::EOF_TOKEN:    return "EOF";
    }
    return "UNKNOWN";
}

int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    LexerOptions opt;
    vector<string> args;
    for(int j=1;j<argc;++j){
        string a=argv[j];
        if (a=="--color") opt.color=true;
        else args.push_back(a);
    }

    string input;
    if (!args.empty()){
        ifstream f(args[0], ios::binary);
        if (!f){
            cerr << "Cannot open file: " << args[0] << "\n";
            return 1;
        }
        stringstream ss; ss << f.rdbuf(); input = ss.str();
    } else {
        // зі stdin
        stringstream ss; ss << cin.rdbuf(); input = ss.str();
    }

    Lexer lex(input, opt);

    // пари <лексема, тип> (WHITESPACE пропускаємо)
    // паралельно, якщо --color, виводимо “розфарбований” рядок
    string colored;
    while(true){
        Token t = lex.next();
        if (t.type==TokType::EOF_TOKEN) break;

        if (opt.color){
            if (t.type==TokType::WHITESPACE) colored += t.lexeme;
            else {
                colored += colorFor(t.type) + t.lexeme + "\033[0m";
            }
        }

        if (t.type==TokType::WHITESPACE) continue;

        cout << "< " << t.lexeme << " , " << typeName(t.type);
        if (t.type==TokType::ERROR_TOKEN && !t.err.empty()){
            cout << " | line " << t.line << ", col " << t.col << " | " << t.err;
        }
        cout << " >\n";
    }

    if (opt.color) {
        cerr << "----- Colored preview (ANSI) -----\n";
        cerr << colored << "\n\033[0m";

        cerr << "\nLegend:\n";
        cerr << "\033[95mKEYWORD/BOOLEAN_NULL\033[0m\n";
        cerr << "\033[94mIDENTIFIER\033[0m\n";
        cerr << "\033[96mNUMBER\033[0m\n";
        cerr << "\033[92mSTRING/CHAR\033[0m\n";
        cerr << "\033[90mCOMMENT\033[0m\n";
        cerr << "\033[33mOPERATOR/DELIMITER\033[0m\n";
        cerr << "\033[93mANNOTATION\033[0m\n";
        cerr << "\033[91mPREPROCESSOR\033[0m\n";
        cerr << "\033[41;37mERROR\033[0m\n";
    }


    return 0;
}
