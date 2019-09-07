#ifndef _GAMEPLOTSUMMARY_H_
#define _GAMEPLOTSUMMARY_H_

#include <string>
#include <vector>
#include "2d/CCLabel.h"
#include "2d/CCNode.h"

using namespace std;

class GamePlotSummary:public cocos2d::Node
{
public:
	static GamePlotSummary* create(string str);
	GamePlotSummary(string str);
	~GamePlotSummary();

private:
	void updateStr(float cTime);
	string subUTF8(const string &str, int from, int to);
	void pareseUTF8(const string &str);

private:
	string content;
	vector<string> test;
	cocos2d::Label* pLabel;
	int wordCount;
	int contentLength;
	float time;
};

#endif

