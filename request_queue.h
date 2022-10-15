#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
                                            RequestQueue(const SearchServer& search_server);

    using FindResult = std::vector<Document>;

    template <typename DocumentPredicate>
    FindResult                                      AddFindRequest(const std::string& raw_query,
        DocumentPredicate document_predicate);

    FindResult                                      AddFindRequest(const std::string& raw_query,
        DocumentStatus status);

    FindResult                                      AddFindRequest(const std::string& raw_query);

    int                                             GetNoResultRequests() const;

private:
    struct QueryResult {
        uint64_t timestamp;
        int results;
    };

private:
    std::deque<QueryResult>                         requests_;
    const SearchServer& search_server_;
    int                                             no_result_requests_ = 0;
    uint64_t                                        current_time_ = 0;
    const static int                                minute_in_day_ = 1440;

private:
    void                                            AddRequest(int count_results);
};

template <typename DocumentPredicate>
RequestQueue::FindResult RequestQueue::AddFindRequest(const std::string& raw_query,
    DocumentPredicate document_predicate) {
    const auto documents = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddRequest(documents.size());
    return documents;
}