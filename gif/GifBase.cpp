#include "GifBase.h"
#include "ui\UILayout.h"

GifBase::GifBase()
{
	//addevent
	_eventDispatcher->removeEventListener(_touchListener);

	_touchListener = EventListenerTouchOneByOne::create();
	CC_SAFE_RETAIN(_touchListener);
	_touchListener->setSwallowTouches(true);
	_touchListener->onTouchBegan = CC_CALLBACK_2(GifBase::onTouchBegan, this);
	_touchListener->onTouchEnded = CC_CALLBACK_2(GifBase::onTouchEnded, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(_touchListener, this);
}

GifBase::~GifBase()
{
	_eventDispatcher->removeEventListener(_touchListener);
	CC_SAFE_RELEASE_NULL(_touchListener);
}

Texture2D* GifBase::createTexture(Bitmap* bm, int index, bool getCache)
{
    auto textureCache = Director::getInstance()->getTextureCache();
	std::string textureName = getGifFrameName(index);
	Texture2D* texture = NULL;

	if(getCache)
	{
		texture = textureCache->getTextureForKey(textureName.c_str());
		if(texture) return texture;
	}

	if(bm == NULL
		|| ! bm->isValid()
		|| index == UNINITIALIZED_UINT)
	{
		return NULL;
	}
    
    Image* img = new Image();
    
	do
	{
		bool res = true;
		const uint32_t* RgbaData = bm->getRGBA();
        
        res = img->initWithRawData((unsigned char *)RgbaData,bm->getPixelLenth() ,bm->m_width, bm->m_hight, 8);
		if(!res) break;

        textureCache->removeTextureForKey(textureName.c_str());
        
        //Adding texture to CCTextureCache  to ensure that on the Android platform, when cut into the foreground from the background, the VolatileTexture can reload our texture
		texture = textureCache->addImage(img, textureName.c_str());
	} while (0);
    
    CC_SAFE_RELEASE(img);

	return texture;
}

bool GifBase::onTouchBegan(Touch *touch, Event *unusedEvent)
{
	Point _touchStartPos = touch->getLocation();
	Point nsp = convertToNodeSpace(_touchStartPos);
	Rect bb;
	bb.size = _contentSize;
	if (bb.containsPoint(nsp) && isClippingParentContainsPoint(_touchStartPos) && isAncestorsVisible(this))
	{

		return true;
	}
	return false;
}

void GifBase::onTouchEnded(Touch *touch, Event *unusedEvent)
{
	Point _touchStartPos = touch->getLocation();
	Point nsp = convertToNodeSpace(_touchStartPos);
	Rect bb;
	bb.size = _contentSize;
	if (bb.containsPoint(nsp) && isClippingParentContainsPoint(_touchStartPos) && isAncestorsVisible(this))
	{
		if (_gifEventCallback)
			_gifEventCallback();
	}
}

bool GifBase::isAncestorsVisible(Node* node)
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

bool GifBase::isClippingParentContainsPoint(const Vec2 &pt)
{
	bool affectByClipping = false;

	Node* parent = getParent();
	ui::Widget* clippingParent = nullptr;
	while (parent)
	{
		ui::Layout* layoutParent = dynamic_cast<ui::Layout*>(parent);
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
