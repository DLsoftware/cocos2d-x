#pragma once
#pragma execution_character_set("utf-8")
#ifndef __LINK_LABELEX__
#define __LINK_LABELEX__

#include "CCLuaValue.h"
#include "CCLuaEngine.h"
#include "2d/CCLabel.h"
#include "2d/CCLayer.h"
#include "base/CCEventDispatcher.h"
#include "base/CCEventListenerTouch.h"
#include "base/CCEventListenerMouse.h"
#include <list>

USING_NS_CC;
using namespace std;

class LinkLabelEx;



class LabelContain : Node
{
public:
	static LabelContain* create(const char* text, const Color4B& color, const char* fontName, float fontSize);
	void enableLinkLine(const Color4B& linkcolor, GLubyte linksize,
		const Color4B& inColor = Color4B::RED, const Color4B& clickColor = Color4B::RED);

	
	void addClickEvent(LUA_FUNCTION fun){_clickHandler = fun;}
	void setString(const char* text);
	void setBoindingData(const char* data){_bindData.assign(data);}
	void setChangeLineEnabled(bool value){ _isChangeLine = value; }
	void setContentSize(const Size& contentSize) override;
	Size getContentSize(){ return _contentSize; }
protected:
	LabelContain();
	~LabelContain();

	bool init(const char* text, const Color4B& color, const char* fontName, float fontSize);
	virtual void visit(Renderer *renderer, const Mat4 &parentTransform, uint32_t parentFlags) override;
	void pushToContainer(LinkLabelEx* lab);
	void displayLabel();
	void handleTextRenderer(const char* text);
	LinkLabelEx* createLabel(const char* text);

	bool onTouchBegan(Touch *touch, Event *unusedEvent);
	void onTouchEnded(Touch *touch, Event *unusedEvent);
	void onMouseMove(Event *event);
	
private:

	EventListenerTouchOneByOne* _touchListener;
	EventListenerMouse*		_mouseListener;
	LUA_FUNCTION _clickHandler;

	std::vector<LinkLabelEx*> _labelVector;

	std::string _text;
	std::string _fontName;
	std::string _bindData;
	
	Color4B _labColor;
	Color4B _outcolor;
	Color4B _linkcolor;
	Color4B _inColor;
	Color4B _clickColor;
	GLubyte _linksize;
	GLubyte _outlinesize;

	Size _contentSize;

	float _fontSize;
	float _leftSpaceWidth;
	bool _isReflash;
	bool _isChangeLine;

	bool _isMoveIn;

};



class LinkLabelEx : public Label
{
public:
	static LinkLabelEx* create(const std::string& text, std::string& fontFile, float fontSize, const Size& dimensions = Size::ZERO,
		TextHAlignment hAlignment = TextHAlignment::LEFT, TextVAlignment vAlignment = TextVAlignment::TOP);

	void enableLinkLine(const Color4B& linkcolor, GLubyte linksize,
		const Color4B& inColor = Color4B::RED, const Color4B& clickColor = Color4B::RED);

protected:
	LinkLabelEx(FontAtlas *atlas = nullptr, TextHAlignment hAlignment = TextHAlignment::LEFT,
		TextVAlignment vAlignment = TextVAlignment::TOP, bool useDistanceField = false, bool useA8Shader = false);
	virtual ~LinkLabelEx();

	LayerColor* getLinkLine(){ return _linkline; }

	bool isAncestorsVisible(Node* node);
	bool isClippingParentContainsPoint(const Vec2 &pt);
	void beClickHandler();
	void setHighlight(bool value);
	void setLabelExColor(const Color4B& color);
private:
	
	LayerColor* _linkline;
	Color4B _labColor;
	Color4B _linkcolor;
	Color4B _inColor;
	Color4B _clickColor;
	GLubyte _linksize;
	bool _isMouseIn;

	friend class LabelContain;
};


#endif
