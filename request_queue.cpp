#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server) {}

RequestQueue::FindResult RequestQueue::AddFindRequest(const string& raw_query,
    DocumentStatus status) {
    const auto documents = search_server_.FindTopDocuments(raw_query, status);
    AddRequest(documents.size());
    return documents;
}

RequestQueue::FindResult RequestQueue::AddFindRequest(const string& raw_query) {
    const auto documents = search_server_.FindTopDocuments(raw_query);
    AddRequest(documents.size());
    return documents;
}

int RequestQueue::GetNoResultRequests() const {
    return no_result_requests_;
}

void RequestQueue::AddRequest(int count_results) {
    ++current_time_;
    while (!requests_.empty() && minute_in_day_ <= current_time_ - requests_.front().timestamp) {
        if (requests_.front().results == 0) {
            --no_result_requests_;
        }
        requests_.pop_front();
    }
    requests_.push_back(QueryResult{ current_time_, count_results });
    if (count_results == 0) {
        ++no_result_requests_;
    }
}