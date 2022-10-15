#pragma once

#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <unordered_set>

using namespace std;

vector<string> SplitIntoWords(string text);
std::vector<std::string_view> SplitIntoWordsView(std::string_view text);

template <typename StringContainer>
unordered_set<string_view> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    unordered_set<string_view> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}