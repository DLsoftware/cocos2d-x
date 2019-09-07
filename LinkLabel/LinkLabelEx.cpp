#pragma once
#pragma execution_character_set("utf-8")
#include "LinkLabelEx.h"
#include "base/CCDirector.h"
#include "platform/CCFileUtils.h"
#include "ui/UILayout.h"
#include "ui/UIWidget.h"
#include "textutil.h"
#include "2d/CCSprite.h"
//#include "Tools.h"

using namespace ui;



LabelContain::LabelContain()
{
	_mouseListener = nullptr;
	_touchListener = nullptr;
	_contentSize = Size(0, 0);
	_isReflash = true;
	_isChangeLine = false;
	_outlinesize = 0;
	_linksize = 0;
	_leftSpaceWidth = 0;
	_clickHandler = 0;
	_isMoveIn = false;
}

LabelContain::~LabelContain()
{
	for each (LinkLabelEx *lab in _labelVector)
	{
		lab->removeFromParent();
	}

	_labelVector.clear();
	_labelVector.shrink_to_fit();
	if (_linksize)
	{
		_eventDispatcher->removeEventListener(_touchListener);
		CC_SAFE_RELEASE_NULL(_touchListener);
		_eventDispatcher->removeEventListener(_mouseListener);
		CC_SAFE_RELEASE_NULL(_mouseListener);
	}
	_clickHandler = 0;
}


LabelContain* LabelContain::create(const char* text, const Color4B& color, const char* fontName, float fontSize)
{
	auto ret = new LabelContain();
	if (ret && ret->init(text, color, fontName, fontSize))
	{
		ret->autorelease();
		return ret;
	}
	CC_SAFE_DELETE(ret);
	return NULL;
}

bool LabelContain::init(const char* text, const Color4B& color, const char* fontName, float fontSize)
{
	if (Node::init())
	{
		_text = text;
		_labColor = color;
		_fontName = fontName;
		_fontSize = fontSize;
		return true;
	}
	return false;
}


void LabelContain::setString(const char* text)
{
	_text = text;
	_isReflash = true;
}


void LabelContain::visit(Renderer *renderer, const Mat4 &parentTransform, uint32_t parentFlags)
{
	Node::visit(renderer, parentTransform, parentFlags);
	if (_isReflash)
	{
		for each (LinkLabelEx *lab in _labelVector)
		{
			lab->removeFromParent();
		}
		_labelVector.clear();
		displayLabel();

		_isReflash = false;
	}
}

void LabelContain::pushToContainer(LinkLabelEx* lab)
{
	_labelVector.push_back(lab);
}

void LabelContain::displayLabel()
{
	handleTextRenderer(_text.c_str());

	float rec_y = 0;
	float maxH = 0;
	float maxW = 0;
	for (size_t i = 0; i < _labelVector.size(); i++)
	{
		LinkLabelEx* textRenderer = _labelVector.at(i);
		std::string tmpStr = textRenderer->getString();
		float ht = textRenderer->getContentSize().height;
		textRenderer->setPosition(0, rec_y);
		textRenderer->setAnchorPoint(Vec2(0,1));
		this->addChild(textRenderer);

		rec_y -= ht;
		maxH += ht;
		maxW = MAX(maxW, textRenderer->getContentSize().width);

		LayerColor* link = textRenderer->getLinkLine();
		if (link)
		{
			this->addChild(link);
			link->setPosition(0, rec_y);
			rec_y -= _linksize;

			maxH += _linksize;
		}
	}
	setContentSize(Size(maxW, maxH));
}

void LabelContain::handleTextRenderer(const char* text)
{
	LinkLabelEx* textRenderer = createLabel(text);
	float textRendererWidth = textRenderer->getContentSize().width;
	_leftSpaceWidth -= textRendererWidth;
	if (_leftSpaceWidth < 0.0f && _isChangeLine == true)
	{
		float leftRendererWidth = textRendererWidth + _leftSpaceWidth;
		std::string curText = text;

		int leftLength = getStringSplitPos(curText, textRendererWidth, leftRendererWidth);
		char* leftWords = new char[leftLength + 1];
		memcpy(leftWords, text, leftLength);
		leftWords[leftLength] = '\0';

		int rightLength = curText.length() - leftLength;
		char* curWords = new char[rightLength + 1];
		memcpy(curWords, text + leftLength, rightLength);
		curWords[rightLength] = '\0';

		/*const std::string tmpStr1 = Utf8ToMulti(leftWords);
		const std::string tmpStr2 = Utf8ToMulti(cutWords);*/

		if (leftLength > 0)
		{
			LinkLabelEx* leftRenderer = createLabel(leftWords);
			if (leftRenderer)
			{
				leftRenderer->setLabelExColor(_labColor);
				leftRenderer->setOpacity(255);
				pushToContainer(leftRenderer);;
			}
		}
		_leftSpaceWidth = _contentSize.width;
		handleTextRenderer(curWords);

		free(leftWords);
		free(curWords);
	}
	else
	{
		//const std::string tmpStr2 = Utf8ToMulti(text);
		textRenderer->setLabelExColor(_labColor);
		textRenderer->setOpacity(255);
		pushToContainer(textRenderer);
	}
}

LinkLabelEx* LabelContain::createLabel(const char* text)
{
	LinkLabelEx* textRenderer = LinkLabelEx::create(text, _fontName, _fontSize);
	if (textRenderer)
	{
		if (_outlinesize > 0)
		{
			textRenderer->enableOutline(_outcolor, _outlinesize);
		}
		if (_linksize > 0)
		{
			textRenderer->enableLinkLine(_linkcolor, _linksize, _inColor, _clickColor);
		}
	}
	
	return textRenderer;
}

void LabelContain::setContentSize(const Size& contentSize)
{
	_contentSize = contentSize;
	_leftSpaceWidth = _contentSize.width;
}

void LabelContain::enableLinkLine(const Color4B& linkcolor, GLubyte linksize, const Color4B& inColor /*= Color4B::RED*/, const Color4B& clickColor /*= Color4B::RED*/)
{
	_linkcolor = linkcolor;
	_linksize = linksize;
	_inColor = inColor;
	_clickColor = clickColor;

	if (_linksize > 0)
	{
		//_eventDispatcher = cocos2d::Director::getInstance()->getEventDispatcher();
		_eventDispatcher->removeEventListener(_touchListener);
		_eventDispatcher->removeEventListener(_mouseListener);

		_touchListener = EventListenerTouchOneByOne::create();
		CC_SAFE_RETAIN(_touchListener);
		_touchListener->setSwallowTouches(true);
		_touchListener->onTouchBegan = CC_CALLBACK_2(LabelContain::onTouchBegan, this);
		_touchListener->onTouchEnded = CC_CALLBACK_2(LabelContain::onTouchEnded, this);
		//_eventDispatcher->addEventListenerWithFixedPriority(_touchListener, -1);
		_eventDispatcher->addEventListenerWithSceneGraphPriority(_touchListener, this);

		_mouseListener = EventListenerMouse::create();
		CC_SAFE_RETAIN(_mouseListener);
		_mouseListener->onMouseMove = CC_CALLBACK_1(LabelContain::onMouseMove, this);
		_eventDispatcher->addEventListenerWithSceneGraphPriority(_mouseListener, this);
	}
}


bool LabelContain::onTouchBegan(Touch *touch, Event *unusedEvent)
{
	Point _touchStartPos = touch->getLocation();
	Rect bb;

	for (unsigned int i = 0; i < _labelVector.size(); i++)
	{
		LinkLabelEx* lab = _labelVector.at(i);
		Point nsp = lab->convertToNodeSpace(_touchStartPos);
		bb.size = lab->getContentSize();
		if (bb.containsPoint(nsp) && lab->isClippingParentContainsPoint(_touchStartPos) && lab->isAncestorsVisible(this))
		{
			for (unsigned int j = 0; j < _labelVector.size(); j++)
			{
				LinkLabelEx* lab = _labelVector.at(j);
				lab->beClickHandler();
			}

			return true;
		}
	}

	return false;
}

void LabelContain::onTouchEnded(Touch *touch, Event *unusedEvent)
{
	//都派发事件  触发高亮
	EventMouse event(EventMouse::MouseEventType::MOUSE_MOVE);
	Point pos = touch->getLocation();
	event.setCursorPosition(pos.x, pos.y);
	_eventDispatcher->dispatchEvent(&event);

	if (_clickHandler)
	{
		LuaStack* pstack = LuaEngine::getInstance()->getLuaStack();
		pstack->pushString(_bindData.c_str(), _bindData.length());
		pstack->executeFunctionByHandler(_clickHandler, 1);
	}
}

void LabelContain::onMouseMove(Event *event)
{
	EventMouse* e = (EventMouse*)event;
	Vec2 p = e->getLocation();
	p = Director::getInstance()->convertToGL(p);
	Rect bb;

	bool isMoveIn = false;
	for (unsigned int i = 0; i < _labelVector.size(); i++)
	{
		LinkLabelEx* lab = _labelVector.at(i);
		Point nsp = lab->convertToNodeSpace(p);
		bb.size = lab->getContentSize();
		if (bb.containsPoint(nsp) && lab->isClippingParentContainsPoint(p) && lab->isAncestorsVisible(this))
		{
			isMoveIn = true;
		}
	}

	for (unsigned int i = 0; i < _labelVector.size(); i++)
	{
		LinkLabelEx* lab = _labelVector.at(i);
		lab->setHighlight(isMoveIn);
	}

	if (_isMoveIn != isMoveIn)
	{
		_isMoveIn = isMoveIn;
		if (isMoveIn ){
			//CTools::setSystemCursor(sys_cursor_type::cur_hand);
		}else{
			//CTools::setSystemCursor(sys_cursor_type::cur_def);
		}
	}
}


LinkLabelEx::LinkLabelEx(FontAtlas *atlas, TextHAlignment hAlignment, TextVAlignment vAlignment, bool useDistanceField, bool useA8Shader)
: Label(atlas, hAlignment, vAlignment, useDistanceField, useA8Shader), _isMouseIn(false), 
_linkcolor(Color4B::WHITE), _linksize(0), _linkline(NULL)
{

}

LinkLabelEx::~LinkLabelEx()
{
}

LinkLabelEx* LinkLabelEx::create(const std::string& text, std::string& fontFile, float fontSize, const Size& dimensions /*= Size::ZERO*/, 
	TextHAlignment hAlignment /*= TextHAlignment::LEFT*/, TextVAlignment vAlignment /*= TextVAlignment::TOP*/)
{
	auto ret = new LinkLabelEx(nullptr, hAlignment, vAlignment);
	if (ret)
	{
		if (FileUtils::getInstance()->isFileExist(fontFile))
		{
			TTFConfig ttfConfig(fontFile.c_str(), fontSize, GlyphCollection::DYNAMIC);
			if (ret->setTTFConfig(ttfConfig))
			{
				ret->setDimensions(dimensions.width, dimensions.height);
				ret->setString(text);
				ret->autorelease();

				return ret;
			}
		}
		else
		{
			ret->setSystemFontName(fontFile.c_str());
			ret->setSystemFontSize(fontSize);
			ret->setDimensions(dimensions.width, dimensions.height);
			ret->setString(text);
			ret->autorelease();

			return ret;
		}
	}
	CC_SAFE_DELETE(ret);
	return ret;
}

void LinkLabelEx::enableLinkLine(const Color4B& linkcolor, GLubyte linksize, const Color4B& inColor /*= Color4B::RED*/, const Color4B& clickColor /*= Color4B::RED*/)
{
	_linkcolor = linkcolor;
	_linksize = linksize;
	_inColor = inColor;
	_clickColor = clickColor;
	if (_linksize > 0)
	{
		_linkline = LayerColor::create(_linkcolor);
		_linkline->setContentSize(Size(getContentSize().width, _linksize));
	}
}

void LinkLabelEx::beClickHandler()
{
	this->setTextColor(_clickColor);
	_linkline->initWithColor(_clickColor, getContentSize().width, _linksize);
}

void LinkLabelEx::setHighlight(bool value)
{
	if (value)
	{
		this->setTextColor(_inColor);
		_linkline->initWithColor(_inColor, getContentSize().width, _linksize);
	}
	else
	{
		this->setTextColor((Color4B)_labColor);
		_linkline->initWithColor(_linkcolor, getContentSize().width, _linksize);
	}
}

bool LinkLabelEx::isAncestorsVisible(Node* node)
{
	if (nullptr == node)
	{
		return true;
	}
	Node* parent = node->getParent();

	if (parent && !parent->isVisible())
	{
		return false;
	}
	return this->isAncestorsVisible(parent);
}

bool LinkLabelEx::isClippingParentContainsPoint(const Vec2 &pt)
{
	bool affectByClipping = false;

	Node* parent = getParent();
	Widget* clippingParent = nullptr;
	while (parent)
	{
		Layout* layoutParent = dynamic_cast<Layout*>(parent);
		if (layoutParent)
		{
			if (layoutParent->isClippingEnabled())
			{
				affectByClipping = true;
				clippingParent = layoutParent;
				break;
			}
		}
		parent = parent->getParent();
	}

	if (!affectByClipping)
	{
		return true;
	}


	if (clippingParent)
	{
		bool bRet = false;
		if (clippingParent->hitTest(pt))
		{
			bRet = true;
		}
		if (bRet)
		{
			return clippingParent->isClippingParentContainsPoint(pt);
		}
		return false;
	}
	return true;
}

void LinkLabelEx::setLabelExColor(const Color4B& color)
{
	Label::setTextColor(color);
	_labColor = color;
}
