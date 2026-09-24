#include "use_cases_impl.h"

#include <stdexcept>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name is empty");
    }
    authors_.Save({AuthorId::New(), name});
}

std::vector<AuthorInfo> UseCasesImpl::GetAuthors() {
    std::vector<AuthorInfo> result;
    for (const auto& author : authors_.GetAllAuthors()) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year) {
    books_.Save({BookId::New(), AuthorId::FromString(author_id), title, publication_year});
}

std::vector<BookInfo> UseCasesImpl::GetBooks() {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetAllBooks()) {
        result.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetAuthorBooks(AuthorId::FromString(author_id))) {
        result.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return result;
}

}  // namespace app
