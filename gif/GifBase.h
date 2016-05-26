#ifndef GIFBASE_H
#define GIFBASE_H
#include "cocos2d.h"
#include "Bitmap.h"
#include "base/CCEventListenerTouch.h"
using namespace cocos2d;

typedef std::function<void()> ccGifCallback;

class GifBase : public cocos2d::Sprite
{
public:
	virtual void addEventListener(const ccGifCallback& callback){
		_gifEventCallback = callback;
	};

protected:
	GifBase();
	virtual ~GifBase();

	virtual void updateGif(uint32_t delta) = 0;
	virtual cocos2d::Texture2D* createTexture(Bitmap* bm, int index, bool getCache);

	virtual void update(float delta)
	{
		//if delta>1, generally speaking  the reason is the device is stuck
		if (delta > 1)
		{
			return;
		}
		uint32_t ldelta = (uint32_t)(delta * 1000);
		updateGif(ldelta);
	};

	virtual std::string getGifFrameName(int index)
	{
		return m_gif_fullpath;
	};

	bool isAncestorsVisible(Node* node);
	bool isClippingParentContainsPoint(const Vec2 &pt);
	bool onTouchBegan(Touch *touch, Event *unusedEvent);
	void onTouchEnded(Touch *touch, Event *unusedEvent);

	std::string m_gif_fullpath;

	ccGifCallback _gifEventCallback;

	EventListenerTouchOneByOne* _touchListener;
};
#endif//GIFBASE_H

