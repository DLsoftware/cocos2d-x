

local MyRichText = class("MyRichText", function ()
	return ccui.Widget:create()
end)

function MyRichText:ctor()
	self._formatTextDirty = true
	self._leftSpaceWidth = 0
	self._verticalSpace = 0
	self._contentSize = cc.size(100,100)
	self._elementRenderersContainer = nil
	self._richElements = {}
	self._elementRenders = {}

	self:initRenderer()
end

function MyRichText:initRenderer()
	self._elementRenderersContainer = cc.Node:create()
	self._elementRenderersContainer:setAnchorPoint(0, 1)
    self._elementRenderersContainer:setContentSize(0,0)
	self:addProtectedChild(self._elementRenderersContainer, 0, -1);
end

function MyRichText:pushBackElement(element)
	table.insert(self._richElements, element)
	self._formatTextDirty = true
end

function MyRichText:insertElement(element, index)
	table.insert(self._richElements, index, element)
	self._formatTextDirty = true
end

function MyRichText:removeElementByIndex(index)
	table.remove(self._richElements, index)
	self._formatTextDirty = true
end

function MyRichText:removeElement(element)
	for i,v in ipairs(self._richElements) do
		if v == element then
			table.remove(self._richElements, i)
			return
		end
	end
	self._formatTextDirty = true
end

function MyRichText:formatText()
	if self._formatTextDirty == false then
		return
	end

	self._formatTextDirty = false

	self._elementRenderersContainer:removeAllChildren()
	self._elementRenders = {}

	self:addNewLine()

	for i,element in ipairs(self._richElements) do

		local elementRenderer = nil
		if element._type == element.Type["TEXT"] then

			self:handleTextRenderer(element._text, element._fontName, element._fontSize, element._color, element._opacity)

		elseif element._type == element.Type["IMAGE"] then

		elseif element._type == element.Type["CUSTOM"] then

		end
	end
	self:formarRenderers()
end

function MyRichText:getUf8TextSplitLen(text, splitTxtWidth, stringLength)
	local ConfigFontsPixWidth = require("myRichText.ConfigFontsPixWidth")
	local pixW = 0
	for i=1,stringLength do
		local word = MySplitStrTools.utf8sub(text, i, 1)
		local char = string.byte(word, 1)
		local w = ConfigFontsPixWidth[char] or ConfigFontsPixWidth["w"]
		pixW = pixW + w

		if pixW > splitTxtWidth then
			return i - 1
		end

        if i == stringLength then
            return stringLength        
        end
	end

	return stringLength
end

function MyRichText:handleTextRenderer(text, fontName, fontSize, color3b, opacity)
	local textRenderer

	if cc.FileUtils:getInstance():isFileExist(fontName) then
		textRenderer = cc.Label:createWithTTF(text, fontName, fontSize)
	else
		textRenderer = cc.Label:createWithSystemFont(text, fontName, fontSize)
	end

	local textRendererWidth = textRenderer:getContentSize().width
	
	if self._leftSpaceWidth < textRendererWidth then
		local curText = text
		local stringLength = MySplitStrTools.utf8len(curText)
		local leftLength = self:getUf8TextSplitLen(curText, self._leftSpaceWidth, stringLength)
		local leftWords = MySplitStrTools.utf8sub(curText, 1, leftLength)
        local cutWords
        if stringLength > leftLength then
		    cutWords = MySplitStrTools.utf8sub(curText, leftLength + 1, stringLength - leftLength)
        else
            cutWords = ""
        end

		if leftLength > 0 then
			local leftRenderer

			if cc.FileUtils:getInstance():isFileExist(fontName) then
				leftRenderer = cc.Label:createWithTTF(leftWords, fontName, fontSize)
			else
				leftRenderer = cc.Label:createWithSystemFont(leftWords, fontName, fontSize)
			end

			if leftRenderer then
				leftRenderer:setColor(color3b)
				leftRenderer:setOpacity(opacity)
				self:pushToContainer(leftRenderer)
			end
		end

		self:addNewLine()
		self:handleTextRenderer(cutWords, fontName, fontSize, color3b, opacity)
	else
		self._leftSpaceWidth = self._leftSpaceWidth - textRendererWidth
		textRenderer:setColor(color3b)
		textRenderer:setOpacity(opacity)
		self:pushToContainer(textRenderer)
	end
end

function MyRichText:handleImageRenderer(fileParh, color3b, opacity)

end

function MyRichText:handleCustomRenderer(renderer)

end

function MyRichText:addNewLine()
	self._leftSpaceWidth = self._contentSize.width
	table.insert(self._elementRenders, {})
end

function MyRichText:formarRenderers()
	local newContentSizeHeight = 0
	local maxHeights = {}

	for i,row in ipairs(self._elementRenders) do
		
		local maxHeight = 0
		for j,v in ipairs(row) do
			maxHeight = math.max(v:getContentSize().height, maxHeight)
		end
		maxHeights[i] = maxHeight
		newContentSizeHeight = newContentSizeHeight + maxHeights[i]
	end

	local nextPosY = 0
	for i,row in ipairs(self._elementRenders) do
		
		local nextPosX = 0


		for j,v in ipairs(row) do
			v:setAnchorPoint(0, 1)
			v:setPosition(nextPosX, nextPosY)
			self._elementRenderersContainer:addChild(v, 1);
			nextPosX = nextPosX + v:getContentSize().width
		end
		
		nextPosY = nextPosY - (maxHeights[i] + self._verticalSpace)
	end

	self:_setContentSize(cc.size(self._contentSize.width, -nextPosY))
    self._elementRenderersContainer:setPositionY(-nextPosY)
end

function MyRichText:pushToContainer(renderer)
	if #self._elementRenders <= 0 then
		return
	end

	table.insert(self._elementRenders[#self._elementRenders], renderer)
end

function MyRichText:setVerticalSpace(space)
    self._verticalSpace = space
end

function MyRichText:setAnchorPoint(px, py)
    self.super.setAnchorPoint(self, px, py)
    self._elementRenderersContainer:setAnchorPoint(px, py)
end

function MyRichText:_setContentSize(s)
	self:setContentSize(s)
	self._contentSize = s
end

function MyRichText:getContentSize()
	return self._contentSize
end

return MyRichText
