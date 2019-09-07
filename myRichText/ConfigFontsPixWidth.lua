 local lab = cc.Label:createWithTTF("", "font/jbwks.ttf", 30)
 local pxTab = {}
 for i=0,127 do
     lab:setString(string.char(i))
     pxTab[i] = lab:getContentSize().width
 end
 lab:setString("我")
 pxTab["w"] = lab:getContentSize().width


local ConfigFontsPixWidth = pxTab

return ConfigFontsPixWidth
