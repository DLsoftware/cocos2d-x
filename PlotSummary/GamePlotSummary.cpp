#include "GamePlotSummary.h"
#include "platform/CCPlatformMacros.h"
#include "base/ccTypes.h"
#include "math/Vec2.h"


GamePlotSummary* GamePlotSummary::create(string str)
{
	GamePlotSummary * ret = new (std::nothrow) GamePlotSummary(str);
	if (ret)
	{
		ret->autorelease();
	}
	else
	{
		CC_SAFE_DELETE(ret);
	}
	return ret;
}


GamePlotSummary::GamePlotSummary(string str) :content(str), wordCount(0), contentLength(0), time(0.0f)
{
	pLabel = cocos2d::Label::createWithSystemFont(str, "Arial", 14);
	pLabel->setAnchorPoint(cocos2d::Vec2(0, 1));
	this->addChild(pLabel);
	this->pareseUTF8(content);
	this->schedule(schedule_selector(GamePlotSummary::updateStr));
}


GamePlotSummary::~GamePlotSummary()
{
}

void GamePlotSummary::updateStr(float cTime)
{
	if (wordCount > contentLength)
	{
		this->unschedule(schedule_selector(GamePlotSummary::updateStr));
		return;
	}

	time += cTime;

	if (time > 0.2f)
	{
		time = 0;
		wordCount++;
		pLabel->setString(subUTF8(content, 0, wordCount));
	}
}

std::string GamePlotSummary::subUTF8(const string &str, int from, int to)
{
	if (from > to) return "";

	if (test.size() < to) return str;

	std::vector<string>::iterator iter = test.begin();
	std::string res;
	std::string result;
	for (iter = (test.begin() + from); iter != (test.begin() + to); iter++)
	{
		res += *iter;

	}
	return res;
}

void GamePlotSummary::pareseUTF8(const string &str)
{
	int l = str.length();


	test.clear();
	for (int p = 0; p < l;)
	{
		int size = 0;
		unsigned char c = str[p];

		//utf8 格式
		if (c < 0x80)//单字节 0xxx xxxx
		{
			size = 1;
		}
		else if (c >= 0xc0 && c < 0xe0)  //双字节 110x xxxx
		{
			size = 2;
		}
		else if (c >= 0xe0 && c < 0xf0)	//三字节  1110 xxxx
		{
			size = 3;
		}
		else
		{
			CC_ASSERT(false);
		}
		std::string temp = "";
		temp = str.substr(p, size);
		test.push_back(temp);
		p += size;
		contentLength++;
	}
}
