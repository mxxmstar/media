// #include "json/json.h"
// #include "boost/lexical_cast.hpp"
#include <json/json.h>
#include <boost/lexical_cast.hpp>
#include <iostream>
using namespace std;
int main() {
    Json::Value root;
    Json::FastWriter fast;
    root["ModuleType"]= Json::Value("你好");
    root["ModuleCode"]= Json::Value("22");
    root["ModuleDesc"]= Json::Value("33");
    root["DateTime"]= Json::Value("44");
    root["LogType"]= Json::Value("55");
    cout<<fast.write(root)<<endl;

    cout << "Enter your weight: ";
    float weight;
    cin >> weight;
    string gain = "A 10% increase raises ";
    string wt = boost::lexical_cast<string> (weight);
    gain = gain + wt + " to ";      // string operator()
    weight = 1.1 * weight;
    gain = gain + boost::lexical_cast<string>(weight) + ".";
    cout << gain << endl;
}