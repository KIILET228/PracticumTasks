#include "book_manager.h"

#include <optional>

namespace db {

using namespace std::literals;
using pqxx::operator"" _zv;

namespace {

constexpr auto kAddBookTag = "add_book"_zv;

// Достаёт из payload строковое поле, учитывая, что в JSON оно может быть null.
std::optional<std::string> GetOptionalString(const boost::json::object& payload, boost::json::string_view key) {
    const auto& value = payload.at(key);
    if (value.is_null()) {
        return std::nullopt;
    }
    return std::string(value.as_string());
}

}  // namespace

BookManager::BookManager(const std::string& db_url)
    : connection_{db_url} {
    CreateTable();
    PrepareStatements();
}

void BookManager::CreateTable() {
    pqxx::work w(connection_);
    w.exec(
        "CREATE TABLE IF NOT EXISTS books ("
        "id SERIAL PRIMARY KEY, "
        "title varchar(100) NOT NULL, "
        "author varchar(100) NOT NULL, "
        "year integer NOT NULL, "
        "ISBN char(13) UNIQUE"
        ");"_zv);
    w.commit();
}

void BookManager::PrepareStatements() {
    connection_.prepare(kAddBookTag,
                        "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)"_zv);
}

bool BookManager::AddBook(const boost::json::object& payload) {
    try {
        const std::string title{payload.at("title").as_string()};
        const std::string author{payload.at("author").as_string()};
        const auto year = static_cast<int>(payload.at("year").as_int64());
        const std::optional<std::string> isbn = GetOptionalString(payload, "ISBN");

        pqxx::work w(connection_);
        // exec_prepared сам экранирует значения, подставляя их вместо
        // плейсхолдеров $1..$4, поэтому запрос устойчив к SQL-инъекциям.
        w.exec_prepared(kAddBookTag, title, author, year, isbn);
        w.commit();
        return true;
    } catch (const pqxx::sql_error&) {
        // Например, нарушение уникальности ISBN - штатная ситуация,
        // о которой сообщаем через {"result": false}.
        return false;
    }
}

boost::json::array BookManager::GetAllBooks() {
    // Запрос без пользовательского ввода, поэтому можно передавать текст
    // напрямую - как и рекомендует библиотека для read_transaction.
    pqxx::read_transaction r(connection_);

    boost::json::array result;
    for (auto [id, title, author, year, isbn] :
        r.query<int, std::string, std::string, int, std::optional<std::string>>(
            "SELECT id, title, author, year, ISBN FROM books "
            "ORDER BY year DESC, title ASC, author ASC, ISBN ASC;"_zv)) {
        boost::json::object book;
        book["id"] = id;
        book["title"] = title;
        book["author"] = author;
        book["year"] = year;
        book["ISBN"] = isbn ? boost::json::value(*isbn) : boost::json::value(nullptr);
        result.push_back(std::move(book));
    }
    return result;
}

}  // namespace db
