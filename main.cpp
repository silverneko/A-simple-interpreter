#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include <deque>
#include <sstream>
#include <map>
#include <cassert>
#include <memory>

#include "Parsers.hpp"

using namespace std;

using Parsers::Parser;
using Parsers::digit;
using Parsers::charp;
using Parsers::anyChar;
using Parsers::oneOf;
using Parsers::satisfy;

class Token{
  public:
    enum Type{Undefine, Comment, Identifier, Constant, LeftBrace, RightBrace, EndOfFile} type;
    std::string cargo;

    Token() : type(Type::Undefine), cargo() {}
    Token(Type t) : type(t), cargo() {}
    Token(const std::string& str) : type(Type::Undefine), cargo(str) {}
    Token(Type t, const std::string& str) : type(t), cargo(str) {}

};

Parser letter(satisfy([](char c){
  if(33 <= c && c <= 39) return true;
  if(42 <= c && c <= 47) return true;
  if(58 <= c && c <= 126) return true;
  return false;
}));

Parser alphaNum(satisfy([](char c){
  if(33 <= c && c <= 39) return true;
  if(42 <= c && c <= 126) return true;
  return false;
}));

Parser space(oneOf(" \t"));
Parser spaces(*space);
Parser skip(spaces);

Parser identifier( letter >> *alphaNum );
Parser constant( +digit );
Parser leftBrace( charp('(') );
Parser rightBrace( charp(')') );

Parser comment( spaces >> charp(';') >> *anyChar );

Parser grammer(Token &token){
  auto f = [&token](Token::Type t){
    return [&token, t](const std::string& str){ token = Token(t, str);};
  };
  return ( identifier[f(Token::Identifier)] 
      | constant[f(Token::Constant)]
      | leftBrace[f(Token::LeftBrace)]
      | rightBrace[f(Token::RightBrace)] );
}

class Tokenizer{
  public:
    istream& inputStream;
    deque<Token> tokens;

    Tokenizer(istream& input = cin) : inputStream(input) {}

    Token getToken(){
      if(tokens.empty()){
        if(! getMoreTokens())
          return Token(Token::EndOfFile);
      }
      Token token = tokens.front();
      tokens.pop_front();
      return token;
    }

    Token peekNextToken(){
      Token token = getToken();
      ungetToken(token);
      return token;
    }

  private:

    void ungetToken(const Token& token){
      tokens.push_front(token);
    }

    bool getMoreTokens(){
      bool flag = false;
      string buffer;
      while(!flag){
        if(! getline(inputStream, buffer)){
          return false;
        }

        bool good;
        auto first = buffer.begin(), last = buffer.end(), second = buffer.begin();

        tie(good, second) = comment.runParser(first, last);
        if(good){
          tokens.push_back(Token(Token::Comment, {first, second}));
          flag = true;
          continue;
        }

        Token token;
        while(first != last){
          tie(ignore, first) = skip.runParser(first, last);
          tie(good, second) = grammer(token).runParser(first, last);
          if(good){
            tokens.push_back(token);
            flag = true;
            swap(first, second);
          }
        }

      }
      return flag;
    }

};

template<typename T>
T fromString(const std::string& str){
  std::istringstream ssin(str);
  T result;
  ssin >> result;
  return result;
}

class Object;
class Context;
class Expression;
using Expressions = std::vector<Expression>;

class Expression{
    static string ws(int i){
      return string(2 * i, ' ');
    }
  public:
    enum Type{Nothing, Ap, Identifier, Constant, Comment} type;
    std::string name;
    Expressions args;

    Expression() : type(Nothing) {}
    Expression(Type t) : type(t) {}
    Expression(const string& str) : type(Nothing), name(str) {}
    Expression(const Expressions& args) : type(Nothing), args(args) {}
    Expression(Type t, const string& str) : type(t), name(str) {}
    Expression(Type t, const Expressions& args) : type(t), args(args) {}

    void print(int = 0) const;
};

Expression getExpression(Tokenizer&);

// Object = Closure | Constant
class Object{
  public:
    using Func = function<Object (Context&, const Expressions&)>;
    enum Type {Nothing, Closure, Constant, Thunk};
  private:
    Type _type;
    int _value;
    Func _func;
    Expression _thunk;
  public:
    Object() : _type(Nothing) {}

    Object(int i) : _type(Constant), _value(i) {}
    Object(const Func& func) : _type(Closure), _func(func) {}
    Object(const Expression& thunk) : _type(Thunk), _thunk(thunk) {}

    Type type() const { return _type;}

    Object operator () (Context& context, const Expressions& args){
      assert(_type == Closure);
      return _func(context, args);
    }

    int& getI(){
      assert(_type == Constant);
      return _value;
    }

    Func& getF(){
      assert(_type == Closure);
      return _func;
    }

    Expression& getT(){
      assert(_type == Thunk);
      return _thunk;
    }
};

// Context :: String -> Object
class Context{
  public:
    std::map<std::string, Object> _table;

    const Object& operator [] (const string& name) const{
      return _table.at(name);
    }

    Context& add(const string& str, int i){
      _table[str] = Object(i);
      return *this;
    }

    Context& add(const string& str, const Object::Func& f){
      _table[str] = Object(f);
      return *this;
    }

    Context& add(const string& str, const Object& obj){
      _table[str] = obj;
      return *this;
    }

    Context& add(const Context& context){
      for(const auto& cont : context._table)
        _table[cont.first] = cont.second;
      return *this;
    }
};

// eval :: Context -> Expression -> Object --May also change the context
Object eval(Context&, const Expression&);

void alter(const std::map<std::string, Expression>& args, Expression& expression){
  if(expression.type == Expression::Identifier){
    auto it = args.find(expression.name);
    if(it != args.end())
      expression = it->second;
  }else if(expression.type == Expression::Ap){
    for_each(expression.args.begin(), expression.args.end(), [&args](Expression& expr){
      alter(args, expr);
    });
  }
}

int main(int argc, char *argv[]){
  using Args = const Expressions&;

  Context env;
  env.add("display", [](Context& context, Args args){
    assert(args.size() == 2);
    auto res = eval(context, args[1]);
    cout << res.getI() << endl;
    return 0;
  }).add("+", [](Context& context, Args args){
    assert(args.size() == 3);
    auto lhs = eval(context, args[1]);
    auto rhs = eval(context, args[2]);
    return lhs.getI() + rhs.getI();
  }).add("-", [](Context& context, Args args){
    assert(args.size() == 3);
    auto lhs = eval(context, args[1]);
    auto rhs = eval(context, args[2]);
    return lhs.getI() - rhs.getI();
  }).add("<", [](Context& context, Args args){
    assert(args.size() == 3);
    auto lhs = eval(context, args[1]);
    auto rhs = eval(context, args[2]);
    return lhs.getI() < rhs.getI();
  }).add("begin", [](Context& context, Args args) -> Object{
    assert(args.size() >= 2);
    for(int i = 1; i < args.size() - 1; ++i)
      eval(context, args[i]);
    return eval(context, args.back());
  }).add("if", [](Context& context, Args args) -> Object{
    assert(args.size() == 4);
    auto predicate = eval(context, args[1]);
    if(predicate.getI() != 0)
      return eval(context, args[2]);
    return eval(context, args[3]);
  }).add("define", [](Context& context, Args args) -> Object{
    assert(args.size() == 3);
    auto lhs = args[1].name;
    auto rhs = eval(context, args[2]);
    context.add(lhs, rhs);
    return rhs;
  }).add("lambda", [](Context& env, Args args) -> Object{
    assert(args.size() == 3);
    shared_ptr<Expressions> _vars(new Expressions(args[1].args));
    shared_ptr<Expression> _body(new Expression(args[2]));
    shared_ptr<Context> _env(new Context(env));
    return Object([_env, _vars, _body](Context& context, Args args) mutable {
      // remember that args[0] is the callee itself
      Expressions& vars(*_vars);
      Expression& body(*_body);
      Context env(*_env);
      for(int i = 1; i < args.size(); ++i){
        Object res = eval(context, args[i]);
        env.add(vars[i-1].name, res);
      }
      // Bind variables done
      Context context1(context);
      context1.add(env);
      return eval(context1, body);
    });
  }).add("macro", [](Context& env, Args args) -> Object{
    assert(args.size() == 3);
    Expressions vars = args[1].args;
    Expression body = args[2];
    return Object([vars, body](Context& context, Args args) mutable {
      // remember that args[0] is the callee itself
      map<string, Expression> table;
      for(int i = 1; i < args.size(); ++i){
        table[ vars[i-1].name ] = args[i];
      }
      alter(table, body);
      // Variables substitution done
      return eval(context, body);
    });
  });

  Tokenizer toker(cin);

  while(true){
    Expression root = getExpression(toker); // Expression treeee
    if(root.type == Expression::Nothing)
      break;
    // root.print(); cout << endl;
    eval(env, root);
  }

  return 0;
}

Object eval(Context& env, const Expression& expression){
  if(expression.type == Expression::Identifier){
    return env[ expression.name ];
  }else if(expression.type == Expression::Constant){
    return Object( fromString<int>(expression.name) );
  }else if(expression.type == Expression::Ap){
    auto callee = eval(env, expression.args[0]);
    auto& func = callee.getF();
    return func(env, expression.args);
  }else if(expression.type == Expression::Comment){
    cout << expression.name << endl;
    return Object();
  }
  return Object();
}

Expression getExpression(Tokenizer& toker){
  Token token = toker.getToken();
  if(token.type == Token::Comment){
    return Expression(Expression::Comment, token.cargo);
  }else if(token.type == Token::Constant){
    return Expression(Expression::Constant, token.cargo);
  }else if(token.type == Token::Identifier){
    return Expression(Expression::Identifier, token.cargo);
  }else if(token.type == Token::LeftBrace){
    Token ntok = toker.peekNextToken();
    Expression expr(Expression::Ap);
    while(true){
      ntok = toker.peekNextToken();
      if(ntok.type == Token::RightBrace){
        break;
      }
      expr.args.push_back( getExpression(toker) );
    }
    toker.getToken(); // Discard right brace ")"
    return expr;
  }else if(token.type == Token::EndOfFile){
    return Expression();
  }
  assert(token.type != Token::RightBrace);
  assert(token.type != Token::Undefine);
  assert(false);
  return Expression();
}


void Expression::print(int indent) const {
  switch(type){
    case Nothing:
      cout << endl;
      break;
    case Ap:
      cout << ws(indent) << "Ap" << " (" << endl;
      for(auto expr : args){
        expr.print(indent + 1);
      }
      cout << ws(indent) << ")" << endl;
      break;
    case Identifier:
      cout << ws(indent) << "Identifier" << " \"" << name << "\"" << endl;
      break;
    case Constant:
      cout << ws(indent) << "Constant" << " \"" << name << "\"" << endl;
      break;
    case Comment:
      break;
  }
}

