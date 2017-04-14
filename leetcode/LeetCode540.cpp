/*
 * LeetCode540.cpp
 *
 *  Created on: 2017年4月14日
 *      Author: wenlong
 */

#include <vector>

using namespace std;

class Solution {

public:

	int singleNonDuplicate(vector<int>& nums) {
		int n = nums.size();
		if (n == 1) {
			return nums[0];
		}

		int left = 0, right = n - 1;
		while (left < right) {
			int index = left + (right - left) / 2;

			if (index % 2 == 0) {
				if (nums[index] == nums[index + 1]) {
					left = index + 2;
				} else if (nums[index] == nums[index - 1]) {
					right = index - 2;
				} else {
					break;
				}
			} else {
				if (nums[index] == nums[index + 1]) {
					right = index - 1;
				} else if (nums[index] == nums[index - 1]) {
					left = index + 1;
				} else {
					break;
				}
			}
		}

		return nums[left];
	}
};


