//
// Created by gnilk on 03.06.23.
//

#include "ViewAPI.h"

using namespace gedit;
bool ViewAPI::IsVisible() {
    return view->IsVisible();
}
void ViewAPI::SetVisible(bool bVisible) {
    view->SetVisible(bVisible);
}
