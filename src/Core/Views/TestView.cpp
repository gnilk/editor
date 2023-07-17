//
// Created by gnilk on 11.05.23.
//

#include "logger.h"

#include "Core/Config/Config.h"
#include "Core/ColorRGBA.h"
#include "TestView.h"

using namespace gedit;

void TestView::DrawViewContents() {
    auto &dc = window->GetContentDC();

    auto uiColors = Config::Instance().GetUIColors();
//    auto bgtmp = ColorRGBA::FromRGB(255,0,0);
//    auto fgtmp = ColorRGBA::FromRGB(0,0,255);
    //dc.SetColor(uiColors["gutter_foreground"], uiColors["gutter_background"]);
    //dc.SetColor(fgtmp, bgtmp);
    dc.ResetDrawColors();

    dc.Clear();

    char str[64];
    for(int i=0;i<dc.GetRect().Height();i++) {
        int idxLine = i;
        dc.ClearLine(i);

        snprintf(str, 64, "%6d", idxLine);
        dc.DrawStringAt(0,i,str);
    }
    DrawSplitter(dc.GetRect().Height()-1);


}
void TestView::DrawSplitter(int row) {
    auto &dc = window->GetContentDC();

    auto logger = gnilk::Logger::GetLogger("HSplitViewStatus");
    logger->Debug("DrawSplitter, row=%d, height=%d", row, dc.GetRect().Height());


    dc.ResetDrawColors();

    std::string dummy(dc.GetRect().Width(), ' ');
    std::string statusLine = " GoatEdit V0.1 | ";
    statusLine += "test";

    statusLine += " | ";
    statusLine += "none";

    dc.FillLine(row, kTextAttributes::kInverted, ' ');
    dc.DrawStringWithAttributesAt(0,row, kTextAttributes::kInverted, statusLine.c_str());

    statusLine = "";
    char tmp[32];
    snprintf(tmp,32, "Ln: %d, Col: %d", 10,10);
    statusLine += tmp;

    dc.DrawStringWithAttributesAt(dc.GetRect().Width()-statusLine.size()-4,row, kTextAttributes::kInverted, statusLine.c_str());

}
