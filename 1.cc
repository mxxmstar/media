#include <iostream>
#include <time.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <queue>
#include <d2d1.h>
using namespace std;

string longestDiverseString(int a, int b, int c) {
        string res = "";
        priority_queue<pair<int, char>> pq;
        if(a > 0) pq.push({a, 'a'});
        if(b > 0) pq.push({b, 'b'});
        if(c > 0) pq.push({c, 'c'});
        while(pq.size()){
            auto [remainTimes, chr] = pq.top();
            pq.pop();
            if(res.size() >= 2 && res[res.length() - 1] == chr && res[res.length() - 2] == chr){
                if (pq.empty()) break;
                auto [newOne, newChr] = pq.top();
                pq.pop();
                pq.push({remainTimes, chr});
                if(newOne != 1){
                    pq.push({newOne - 1, newChr});
                }
                res += newChr;
            }
            else{
                res += chr;
                if(remainTimes != 1){
                    pq.push({remainTimes - 1, chr});
                }
            }
        }
        return res;
    }


int main(){
    longestDiverseString(1, 1, 7);
}