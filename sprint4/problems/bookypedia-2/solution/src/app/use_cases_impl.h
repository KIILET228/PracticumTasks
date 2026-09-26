#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book_fwd.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    std::string AddAuthor(const std::string& name) override;
    std::vector<AuthorInfo> GetAuthors() override;
    std::optional<AuthorInfo> FindAuthorByName(const std::string& name) override;
    void DeleteAuthorByName(const std::string& name) override;
    void DeleteAuthorById(const std::string& author_id) override;
    void EditAuthorName(const std::string& author_id, const std::string& new_name) override;

    void AddBook(const std::string& author_id, const std::string& title, int publication_year,
                const std::string& raw_tags) override;
    std::vector<BookInfo> GetBooks() override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) override;
    std::vector<BookInfo> FindBooksByTitle(const std::string& title) override;
    std::optional<BookDetails> GetBookDetails(const std::string& book_id) override;
    void DeleteBook(const std::string& book_id) override;
    void EditBook(const std::string& book_id, std::optional<std::string> new_title,
                 std::optional<int> new_year, const std::string& raw_tags) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
