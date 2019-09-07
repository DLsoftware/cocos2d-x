local fatherClass = import("myRichtext.MyRichElement")
local MyRichElementText = class("MyRichElementText", fatherClass)

function MyRichElementText:ctor(tag, color3B, opacity, text, fontName, fontSize)
    self.super.init(self, tag, color3B, opacity)
    self._type = fatherClass.Type["TEXT"]
    self._text = text
    self._fontName = fontName
    self._fontSize = fontSize
end


return MyRichElementText
