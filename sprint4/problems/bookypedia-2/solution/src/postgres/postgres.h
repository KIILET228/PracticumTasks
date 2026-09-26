#pragma once
#include <optional>
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    void Delete(const domain::AuthorId& id) override;
    std::vector<domain::Author> GetAllAuthors() const override;
    std::optional<domain::Author> GetByName(const std::string& name) const override;
    std::optional<domain::Author> GetById(const domain::AuthorId& id) const override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Book& book) override;
    void Delete(const domain::BookId& id) override;
    void UpdateTitleAndYear(const domain::BookId& id, const std::string& title, int year) override;
    void SetTags(const domain::BookId& id, const std::vector<std::string>& tags) override;
    std::vector<std::string> GetTags(const domain::BookId& id) const override;
    std::vector<domain::Book> GetAllBooks() const override;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) const override;
    std::vector<domain::Book> FindByTitle(const std::string& title) const override;
    std::optional<domain::Book> GetById(const domain::BookId& id) const override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & {
        return authors_;
    }

    BookRepositoryImpl& GetBooks() & {
        return books_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres
