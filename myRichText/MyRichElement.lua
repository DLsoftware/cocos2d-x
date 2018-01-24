local MyRichElement = class("MyRichElement")

MyRichElement.Type = {
	TEXT = 0,
	IMAGE = 1,
	CUSTOM = 2,
}

function MyRichElement:init(tag, color3B, opacity)
    self._tag = tag
    self._color = color3B
    self._opacity = opacity
end

return MyRichElement
