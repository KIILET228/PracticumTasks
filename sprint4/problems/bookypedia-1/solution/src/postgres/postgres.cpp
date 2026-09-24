#include "postgres.h"

#include <pqxx/pqxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    // Пока каждое обращение к репозиторию выполняется внутри отдельной транзакции
    // В будущих уроках вы узнаете про паттерн Unit of Work, при помощи которого сможете несколько
    // запросов выполнить в рамках одной транзакции.
    // Вы также может самостоятельно почитать информацию про этот паттерн и применить его здесь.
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAllAuthors() const {
    pqxx::read_transaction r{connection_};
    std::vector<domain::Author> authors;
    for (auto [id, name] :
        r.query<std::string, std::string>("SELECT id, name FROM authors ORDER BY name ASC;"_zv)) {
        authors.emplace_back(domain::AuthorId::FromString(id), std::move(name));
    }
    return authors;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(),
        book.GetPublicationYear());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAllBooks() const {
    pqxx::read_transaction r{connection_};
    std::vector<domain::Book> books;
    for (auto [id, author_id, title, year] : r.query<std::string, std::string, std::string, int>(
             "SELECT id, author_id, title, publication_year FROM books ORDER BY title ASC;"_zv)) {
        books.emplace_back(domain::BookId::FromString(id), domain::AuthorId::FromString(author_id),
                           std::move(title), year);
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetAuthorBooks(const domain::AuthorId& author_id) const {
    pqxx::read_transaction r{connection_};

    // Параметризованный запрос - значение author_id подставляется библиотекой,
    // что защищает от SQL-инъекций.
    const auto result = r.exec_params(
        "SELECT id, title, publication_year FROM books "
        "WHERE author_id = $1 "
        "ORDER BY publication_year ASC, title ASC;"_zv,
        author_id.ToString());

    std::vector<domain::Book> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        books.emplace_back(domain::BookId::FromString(row[0].as<std::string>()), author_id,
                           row[1].as<std::string>(), row[2].as<int>());
    }
    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL CONSTRAINT fk_author REFERENCES authors (id),
    title varchar(100) NOT NULL,
    publication_year integer NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE INDEX IF NOT EXISTS books_author_id_idx ON books (author_id);
)"_zv);

    // коммитим изменения
    work.commit();
}

}  // namespace postgres
