#include "process_queries.h"
#include <execution>
#include <numeric>


using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const vector<string>& queries) {
    vector<vector<Document>> result(queries.size());
    transform(execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const auto& query) {return search_server.FindTopDocuments(query); });
    return result;
}

vector <Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const vector<string>& queries) {
    return transform_reduce(execution::par, 
        queries.begin(), queries.end(), 
        vector<Document>(),
        [](vector<Document> lhs, vector<Document> rhs) {
            lhs.insert(lhs.end(), rhs.begin(), rhs.end());
            return lhs; 
        },
        [&search_server](const auto& query) {
            return search_server.FindTopDocuments(query); 
        });
}

