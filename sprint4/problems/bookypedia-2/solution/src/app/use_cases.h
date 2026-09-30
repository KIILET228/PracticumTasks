#pragma once

#include <optional>
#include <string>
#include <vector>

namespace app {

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string id;
    std::string title;
    std::string author_id;
    std::string author_name;
    int publication_year = 0;
};

struct BookDetails {
    std::string title;
    std::string author_name;
    int publication_year = 0;
    std::vector<std::string> tags;  // отсортированы по возрастанию
};

class UseCases {
public:
    // ------- Авторы -------
    virtual std::string AddAuthor(const std::string& name) = 0;
    virtual std::vector<AuthorInfo> GetAuthors() = 0;
    virtual std::optional<AuthorInfo> FindAuthorByName(const std::string& name) = 0;
    virtual void DeleteAuthorByName(const std::string& name) = 0;
    virtual void DeleteAuthorById(const std::string& author_id) = 0;
    virtual void EditAuthorName(const std::string& author_id, const std::string& new_name) = 0;

    // ------- Книги -------
    virtual void AddBook(const std::string& author_id, const std::string& title, int publication_year,
                        const std::string& raw_tags) = 0;
    virtual std::vector<BookInfo> GetBooks() = 0;
    virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) = 0;
    virtual std::vector<BookInfo> FindBooksByTitle(const std::string& title) = 0;
    virtual std::optional<BookDetails> GetBookDetails(const std::string& book_id) = 0;
    virtual void DeleteBook(const std::string& book_id) = 0;
    virtual void EditBook(const std::string& book_id, std::optional<std::string> new_title,
                          std::optional<int> new_year, const std::string& raw_tags) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
