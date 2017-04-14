/*
 * LeetCode535.cpp
 *
 *  Created on: 2017年4月14日
 *      Author: wenlong
 */

#include <string>
#include <map>

using namespace std;

class Solution {
public:

	int count = 0;
	std::map<int, std::string> urlMap;

    // Encodes a URL to a shortened URL.
    string encode(string longUrl) {
    	count++;
    	std::string shortUrl = "http://tnyurl.com/";
    	char xx = count + '0';
    	shortUrl.push_back(xx);
    	urlMap[count] = longUrl;
    	return shortUrl;
    }

    // Decodes a shortened URL to its original URL.
    string decode(string shortUrl) {
    	int val = 0;
    	for (int i = shortUrl.size() - 1; i > 0 && shortUrl[i] != '/'; i--) {
    		val = val * 10 + shortUrl[i] - '0';
    	}
    	return urlMap[val];
    }
};

// Your Solution object will be instantiated and called as such:
// Solution solution;
// solution.decode(solution.encode(url));
