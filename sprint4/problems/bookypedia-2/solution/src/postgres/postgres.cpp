#include "postgres.h"

#include <pqxx/pqxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    // Пока каждое обращение к репозиторию выполняется внутри отдельной транзакции.
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    pqxx::work work{connection_};
    // Схема БД (в частности, при уже существующих таблицах) не гарантирует
    // ON DELETE CASCADE, поэтому вручную удаляем сперва теги книг автора,
    // затем сами книги, и только потом - самого автора.
    work.exec_params(
        "DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id = $1);"_zv,
        id.ToString());
    work.exec_params("DELETE FROM books WHERE author_id = $1;"_zv, id.ToString());
    work.exec_params("DELETE FROM authors WHERE id = $1;"_zv, id.ToString());
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

std::optional<domain::Author> AuthorRepositoryImpl::GetByName(const std::string& name) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params("SELECT id, name FROM authors WHERE name = $1;"_zv, name);
    if (result.empty()) {
        return std::nullopt;
    }
    return domain::Author{domain::AuthorId::FromString(result[0][0].as<std::string>()),
                          result[0][1].as<std::string>()};
}

std::optional<domain::Author> AuthorRepositoryImpl::GetById(const domain::AuthorId& id) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params("SELECT id, name FROM authors WHERE id = $1;"_zv, id.ToString());
    if (result.empty()) {
        return std::nullopt;
    }
    return domain::Author{domain::AuthorId::FromString(result[0][0].as<std::string>()),
                          result[0][1].as<std::string>()};
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

void BookRepositoryImpl::Delete(const domain::BookId& id) {
    pqxx::work work{connection_};
    work.exec_params("DELETE FROM book_tags WHERE book_id = $1;"_zv, id.ToString());
    work.exec_params("DELETE FROM books WHERE id = $1;"_zv, id.ToString());
    work.commit();
}

void BookRepositoryImpl::UpdateTitleAndYear(const domain::BookId& id, const std::string& title, int year) {
    pqxx::work work{connection_};
    work.exec_params("UPDATE books SET title=$1, publication_year=$2 WHERE id=$3;"_zv, title, year,
                     id.ToString());
    work.commit();
}

void BookRepositoryImpl::SetTags(const domain::BookId& id, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    work.exec_params("DELETE FROM book_tags WHERE book_id = $1;"_zv, id.ToString());
    for (const auto& tag : tags) {
        work.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);"_zv, id.ToString(), tag);
    }
    work.commit();
}

std::vector<std::string> BookRepositoryImpl::GetTags(const domain::BookId& id) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params("SELECT tag FROM book_tags WHERE book_id=$1 ORDER BY tag ASC;"_zv,
                                id.ToString());
    std::vector<std::string> tags;
    tags.reserve(result.size());
    for (const auto& row : result) {
        tags.push_back(row[0].as<std::string>());
    }
    return tags;
}

std::vector<domain::Book> BookRepositoryImpl::GetAllBooks() const {
    pqxx::read_transaction r{connection_};
    std::vector<domain::Book> books;
    for (auto [id, author_id, title, year] : r.query<std::string, std::string, std::string, int>(
             "SELECT id, author_id, title, publication_year FROM books;"_zv)) {
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

std::vector<domain::Book> BookRepositoryImpl::FindByTitle(const std::string& title) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params(
        "SELECT id, author_id, title, publication_year FROM books WHERE title = $1;"_zv, title);

    std::vector<domain::Book> books;
    books.reserve(result.size());
    for (const auto& row : result) {
        books.emplace_back(domain::BookId::FromString(row[0].as<std::string>()),
                           domain::AuthorId::FromString(row[1].as<std::string>()),
                           row[2].as<std::string>(), row[3].as<int>());
    }
    return books;
}

std::optional<domain::Book> BookRepositoryImpl::GetById(const domain::BookId& id) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params(
        "SELECT id, author_id, title, publication_year FROM books WHERE id = $1;"_zv, id.ToString());
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result[0];
    return domain::Book{domain::BookId::FromString(row[0].as<std::string>()),
                        domain::AuthorId::FromString(row[1].as<std::string>()), row[2].as<std::string>(),
                        row[3].as<int>()};
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
    title varchar(100) NOT NULL,
    publication_year integer,
    author_id UUID,
    CONSTRAINT fk_author FOREIGN KEY (author_id) REFERENCES authors (id)
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID,
    tag varchar(30) NOT NULL,
    CONSTRAINT fk_book FOREIGN KEY (book_id) REFERENCES books (id)
);
)"_zv);
    work.exec(R"(
CREATE INDEX IF NOT EXISTS books_author_id_idx ON books (author_id);
)"_zv);
    work.exec(R"(
CREATE INDEX IF NOT EXISTS book_tags_book_id_idx ON book_tags (book_id);
)"_zv);

    // коммитим изменения
    work.commit();
}

}  // namespace postgres
