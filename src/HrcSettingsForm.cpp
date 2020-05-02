#include "HrcSettingsForm.h"
#include "tools.h"

HrcSettingsForm::HrcSettingsForm(FarEditorSet* _farEditorSet) : menuid(0)
{
  farEditorSet = _farEditorSet;
  current_filetype = nullptr;
}

bool HrcSettingsForm::Show()
{
  return showForm();
}

INT_PTR WINAPI SettingHrcDialogProc(HANDLE hDlg, intptr_t Msg, intptr_t Param1, void* Param2)
{
  auto* fes = reinterpret_cast<HrcSettingsForm*>(Info.SendDlgMessage(hDlg, DM_GETDLGDATA, 0, nullptr));

  switch (Msg) {
    case DN_INITDIALOG: {
      fes->menuid = -1;
      fes->OnChangeHrc();
      return false;
    }
    case DN_BTNCLICK:
      if (IDX_CH_OK == Param1) {
        fes->OnSaveHrcParams();
        return false;
      }
      break;
    case DN_CONTROLINPUT:
      if (IDX_CH_PARAM_VALUE_LIST == Param1) {
        const auto* record = (const INPUT_RECORD*) Param2;
        if (record->EventType == KEY_EVENT || record->EventType == MOUSE_EVENT) {
          int len = Info.SendDlgMessage(hDlg, DM_GETTEXT, IDX_CH_PARAM_VALUE_LIST, nullptr);
          EditorSetPosition set_pos {};
          set_pos.StructSize = sizeof(EditorSetPosition);
          if (Info.SendDlgMessage(hDlg, DM_GETEDITPOSITION, IDX_CH_PARAM_VALUE_LIST, &set_pos)) {
            if (set_pos.CurPos > len) {
              COORD c {len, 0};
              Info.SendDlgMessage(hDlg, DM_SETCURSORPOS, IDX_CH_PARAM_VALUE_LIST, &c);
            }
          }
        }
      }
      break;
    case DN_EDITCHANGE:
      if (IDX_CH_SCHEMAS == Param1) {
        fes->menuid = -1;
        fes->OnChangeHrc();
        return true;
      }

      break;
    case DN_LISTCHANGE:
      if (IDX_CH_PARAM_LIST == Param1) {
        fes->OnChangeParam(reinterpret_cast<intptr_t>(Param2));
        return true;
      }
      break;
    default:
      break;
  }
  return Info.DefDlgProc(hDlg, Msg, Param1, Param2);
}

bool HrcSettingsForm::showForm()
{
  if (!farEditorSet->Opt.rEnabled) {
    return false;
  }

  FarDialogItem fdi[] = {
      // type, x1, y1, x2, y2, param, history, mask, flags, userdata, ptrdata, maxlen
      {DI_DOUBLEBOX, 2, 1, 56, 21, 0, nullptr, nullptr, 0, nullptr, 0, 0},               // IDX_CH_BOX,
      {DI_TEXT, 3, 3, 0, 3, 0, nullptr, nullptr, 0, nullptr, 0, 0},                      // IDX_CH_CAPTIONLIST,
      {DI_COMBOBOX, 10, 3, 54, 2, 0, nullptr, nullptr, 0, nullptr, 0, 0},                // IDX_CH_SCHEMAS,
      {DI_LISTBOX, 3, 4, 30, 17, 0, nullptr, nullptr, 0, nullptr, 0, 0},                 // IDX_CH_PARAM_LIST,
      {DI_TEXT, 32, 5, 0, 5, 0, nullptr, nullptr, 0, nullptr, 0, 0},                     // IDX_CH_PARAM_VALUE_CAPTION
      {DI_COMBOBOX, 32, 6, 54, 6, 0, nullptr, nullptr, 0, nullptr, 0, 0},                // IDX_CH_PARAM_VALUE_LIST
      {DI_EDIT, 4, 18, 54, 18, 0, nullptr, nullptr, 0, nullptr, 0, 0},                   // IDX_CH_DESCRIPTION,
      {DI_BUTTON, 37, 20, 0, 0, 0, nullptr, nullptr, DIF_DEFAULTBUTTON, nullptr, 0, 0},  // IDX_OK,
      {DI_BUTTON, 45, 20, 0, 0, 0, nullptr, nullptr, 0, nullptr, 0, 0},                  // IDX_CANCEL,
  };

  fdi[IDX_CH_BOX].Data = farEditorSet->GetMsg(mUserHrcSettingDialog);
  fdi[IDX_CH_CAPTIONLIST].Data = farEditorSet->GetMsg(mListSyntax);
  FarList* l = buildHrcList();
  fdi[IDX_CH_SCHEMAS].ListItems = l;
  fdi[IDX_CH_SCHEMAS].Flags = DIF_LISTWRAPMODE | DIF_DROPDOWNLIST;
  fdi[IDX_CH_OK].Data = farEditorSet->GetMsg(mOk);
  fdi[IDX_CH_CANCEL].Data = farEditorSet->GetMsg(mCancel);
  fdi[IDX_CH_PARAM_LIST].Data = farEditorSet->GetMsg(mParamList);
  fdi[IDX_CH_PARAM_VALUE_CAPTION].Data = farEditorSet->GetMsg(mParamValue);
  fdi[IDX_CH_DESCRIPTION].Flags = DIF_READONLY;

  fdi[IDX_CH_PARAM_LIST].Flags = DIF_LISTWRAPMODE | DIF_LISTNOCLOSE;
  fdi[IDX_CH_PARAM_VALUE_LIST].Flags = DIF_LISTWRAPMODE;

  hDlg = Info.DialogInit(&MainGuid, &HrcPluginConfig, -1, -1, 59, 23, L"confighrc", fdi, std::size(fdi), 0, 0, SettingHrcDialogProc, this);
  if (-1 != Info.DialogRun(hDlg)) {
    SaveChangedValueParam();
  }

  removeFarList(l);

  Info.DialogFree(hDlg);
  return true;
}

size_t HrcSettingsForm::getCountFileTypeAndGroup() const
{
  size_t num = 0;
  const String* group = nullptr;
  FileType* type;

  for (int idx = 0;; idx++) {
    type = farEditorSet->hrcParser->enumerateFileTypes(idx);

    if (type == nullptr) {
      break;
    }

    num++;
    if (group != nullptr && !group->equals(type->getGroup())) {
      num++;
    }

    group = type->getGroup();
  }
  return num;
}

FarList* HrcSettingsForm::buildHrcList() const
{
  size_t num = getCountFileTypeAndGroup();
  const String* group = nullptr;
  FileType* type;

  auto* hrcList = new FarListItem[num];
  memset(hrcList, 0, sizeof(FarListItem) * (num));

  for (int idx = 0, i = 0;; idx++, i++) {
    type = farEditorSet->hrcParser->enumerateFileTypes(idx);

    if (type == nullptr) {
      break;
    }

    if (group != nullptr && !group->equals(type->getGroup())) {
      hrcList[i].Flags = LIF_SEPARATOR;
      i++;
    }

    group = type->getGroup();

    const wchar_t* groupChars;

    if (group != nullptr) {
      groupChars = group->getWChars();
    }
    else {
      groupChars = L"<no group>";
    }

    hrcList[i].Text = new wchar_t[255];
    _snwprintf(const_cast<wchar_t*>(hrcList[i].Text), 255, L"%s: %s", groupChars, type->getDescription()->getWChars());
    hrcList[i].UserData = (intptr_t) type;
  }

  hrcList[0].Flags = LIF_SELECTED;
  return buildFarList(hrcList, num);
}

void HrcSettingsForm::OnChangeParam(intptr_t idx)
{
  if (menuid != idx && menuid != -1) {
    SaveChangedValueParam();
  }
  FarListGetItem List {};
  List.StructSize = sizeof(FarListGetItem);
  List.ItemIndex = idx;
  bool res = Info.SendDlgMessage(hDlg, DM_LISTGETITEM, IDX_CH_PARAM_LIST, &List);
  if (!res)
    return;

  menuid = idx;
  CString p = CString(List.Item.Text);

  const String* value;
  value = current_filetype->getParamDescription(p);
  if (value == nullptr) {
    value = farEditorSet->defaultType->getParamDescription(p);
  }
  if (value != nullptr) {
    Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, IDX_CH_DESCRIPTION, (void*) value->getWChars());
  }

  // set visible begin of text
  COORD c {0, 0};
  Info.SendDlgMessage(hDlg, DM_SETCURSORPOS, IDX_CH_DESCRIPTION, &c);

  if (DShowCross.equals(&p)) {
    setCrossValueListToCombobox();
  }
  else {
    if (DCrossZorder.equals(&p)) {
      setCrossPosValueListToCombobox();
    }
    else if (DMaxLen.equals(&p) || DBackparse.equals(&p) || DDefFore.equals(&p) || DDefBack.equals(&p) || CString("firstlines").equals(&p) ||
             CString("firstlinebytes").equals(&p) || DHotkey.equals(&p)) {
      setCustomListValueToCombobox(CString(List.Item.Text));
    }
    else if (DFullback.equals(&p)) {
      setYNListValueToCombobox(CString(List.Item.Text));
    }
    else {
      setTFListValueToCombobox(CString(List.Item.Text));
    }
  }
}

void HrcSettingsForm::OnSaveHrcParams()
{
  SaveChangedValueParam();
  FarHrcSettings p(farEditorSet->parserFactory.get());
  p.writeUserProfile();
}

void HrcSettingsForm::SaveChangedValueParam() const
{
  FarListGetItem List = {0};
  List.StructSize = sizeof(FarListGetItem);
  List.ItemIndex = menuid;
  bool res = Info.SendDlgMessage(hDlg, DM_LISTGETITEM, IDX_CH_PARAM_LIST, &List);

  if (!res)
    return;

  // param name
  CString p = CString(List.Item.Text);
  // param value
  CString v = CString(trim(reinterpret_cast<wchar_t*>(Info.SendDlgMessage(hDlg, DM_GETCONSTTEXTPTR, IDX_CH_PARAM_VALUE_LIST, nullptr))));

  const String* value = current_filetype->getParamUserValue(p);
  const String* def_value = getParamDefValue(current_filetype, p);
  if (value == nullptr || !value->length()) {  ////было default значение
    //если его изменили
    if (!v.equals(def_value)) {
      if (current_filetype->getParamValue(p) == nullptr) {
        current_filetype->addParam(&p);
      }
      current_filetype->setParamValue(p, &v);
    }
  }
  else {                     //было пользовательское значение
    if (!v.equals(value)) {  // changed
      current_filetype->setParamValue(p, &v);
    }
  }

  delete def_value;
}

void HrcSettingsForm::getCurrentTypeInDialog()
{
  auto k = static_cast<int>(Info.SendDlgMessage(hDlg, DM_LISTGETCURPOS, IDX_CH_SCHEMAS, nullptr));
  FarListGetItem f {};
  f.StructSize = sizeof(FarListGetItem);
  f.ItemIndex = k;
  bool res = Info.SendDlgMessage(hDlg, DM_LISTGETITEM, IDX_CH_SCHEMAS, (void*) &f);
  if (res)
    current_filetype = (FileTypeImpl*) f.Item.UserData;
}

void HrcSettingsForm::OnChangeHrc()
{
  if (menuid != -1) {
    SaveChangedValueParam();
  }
  getCurrentTypeInDialog();
  FarList* List = buildParamsList(current_filetype);

  Info.SendDlgMessage(hDlg, DM_LISTSET, IDX_CH_PARAM_LIST, List);
  removeFarList(List);
  OnChangeParam(0);
}

void HrcSettingsForm::setYNListValueToCombobox(CString param) const
{
  const String* value = current_filetype->getParamUserValue(param);
  const String* def_value = getParamDefValue(current_filetype, param);

  size_t count = 3;
  auto* fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem) * (count));
  fcross[0].Text = _wcsdup(DNo.getWChars());
  fcross[1].Text = _wcsdup(DYes.getWChars());
  fcross[2].Text = _wcsdup(def_value->getWChars());
  delete def_value;

  size_t ret = 2;
  if (value == nullptr || !value->length()) {
    ret = 2;
  }
  else {
    if (value->equals(&DNo)) {
      ret = 0;
    }
    else if (value->equals(&DYes)) {
      ret = 1;
    }
  }
  fcross[ret].Flags = LIF_SELECTED;
  ChangeParamValueListType(true);
  auto* lcross = buildFarList(fcross, count);
  Info.SendDlgMessage(hDlg, DM_LISTSET, IDX_CH_PARAM_VALUE_LIST, lcross);
  removeFarList(lcross);
}

void HrcSettingsForm::setTFListValueToCombobox(CString param) const
{
  const String* value = current_filetype->getParamUserValue(param);
  const String* def_value = getParamDefValue(current_filetype, param);

  size_t count = 3;
  auto* fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem) * (count));
  fcross[0].Text = _wcsdup(DFalse.getWChars());
  fcross[1].Text = _wcsdup(DTrue.getWChars());
  fcross[2].Text = _wcsdup(def_value->getWChars());
  delete def_value;

  size_t ret = 2;
  if (value == nullptr || !value->length()) {
    ret = 2;
  }
  else {
    if (value->equals(&DFalse)) {
      ret = 0;
    }
    else if (value->equals(&DTrue)) {
      ret = 1;
    }
  }
  fcross[ret].Flags = LIF_SELECTED;
  ChangeParamValueListType(true);
  auto* lcross = buildFarList(fcross, count);
  Info.SendDlgMessage(hDlg, DM_LISTSET, IDX_CH_PARAM_VALUE_LIST, lcross);
  removeFarList(lcross);
}

void HrcSettingsForm::setCustomListValueToCombobox(CString param) const
{
  const String* value = current_filetype->getParamUserValue(param);
  const String* def_value = getParamDefValue(current_filetype, param);

  size_t count = 1;
  auto* fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem) * (count));
  fcross[0].Text = _wcsdup(def_value->getWChars());
  delete def_value;

  fcross[0].Flags = LIF_SELECTED;
  ChangeParamValueListType(false);
  auto* lcross = buildFarList(fcross, count);
  Info.SendDlgMessage(hDlg, DM_LISTSET, IDX_CH_PARAM_VALUE_LIST, lcross);

  if (value != nullptr) {
    Info.SendDlgMessage(hDlg, DM_SETTEXTPTR, IDX_CH_PARAM_VALUE_LIST, (void*) value->getWChars());
  }
  removeFarList(lcross);
}

void HrcSettingsForm::setCrossValueListToCombobox() const
{
  const String* value = current_filetype->getParamUserValue(DShowCross);
  const String* def_value = getParamDefValue(current_filetype, DShowCross);

  size_t count = 5;
  auto* fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem) * (count));
  fcross[0].Text = _wcsdup(DNone.getWChars());
  fcross[1].Text = _wcsdup(DVertical.getWChars());
  fcross[2].Text = _wcsdup(DHorizontal.getWChars());
  fcross[3].Text = _wcsdup(DBoth.getWChars());
  fcross[4].Text = _wcsdup(def_value->getWChars());
  delete def_value;

  size_t ret = 0;
  if (value == nullptr || !value->length()) {
    ret = 4;
  }
  else {
    if (value->equals(&DNone)) {
      ret = 0;
    }
    else if (value->equals(&DVertical)) {
      ret = 1;
    }
    else if (value->equals(&DHorizontal)) {
      ret = 2;
    }
    else if (value->equals(&DBoth)) {
      ret = 3;
    }
  }
  fcross[ret].Flags = LIF_SELECTED;
  ChangeParamValueListType(true);
  auto* lcross = buildFarList(fcross, count);
  Info.SendDlgMessage(hDlg, DM_LISTSET, IDX_CH_PARAM_VALUE_LIST, lcross);
  removeFarList(lcross);
}

void HrcSettingsForm::setCrossPosValueListToCombobox() const
{
  const String* value = current_filetype->getParamUserValue(DCrossZorder);
  const String* def_value = getParamDefValue(current_filetype, DCrossZorder);

  size_t count = 3;
  auto* fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem) * (count));
  fcross[0].Text = _wcsdup(DBottom.getWChars());
  fcross[1].Text = _wcsdup(DTop.getWChars());
  fcross[2].Text = _wcsdup(def_value->getWChars());
  delete def_value;

  size_t ret = 2;
  if (value == nullptr || !value->length()) {
    ret = 2;
  }
  else {
    if (value->equals(&DBottom)) {
      ret = 0;
    }
    else if (value->equals(&DTop)) {
      ret = 1;
    }
  }
  fcross[ret].Flags = LIF_SELECTED;
  ChangeParamValueListType(true);
  auto* lcross = buildFarList(fcross, count);
  Info.SendDlgMessage(hDlg, DM_LISTSET, IDX_CH_PARAM_VALUE_LIST, lcross);
  removeFarList(lcross);
}

const String* HrcSettingsForm::getParamDefValue(FileTypeImpl* type, SString param) const
{
  const String* value;
  value = type->getParamDefaultValue(param);
  if (value == nullptr) {
    value = farEditorSet->defaultType->getParamValue(param);
  }
  auto* p = new SString("<default-");
  p->append(CString(value));
  p->append(CString(">"));
  return p;
}

FarList* HrcSettingsForm::buildParamsList(FileTypeImpl* type) const
{
  // max count params
  size_t size = type->getParamCount() + farEditorSet->defaultType->getParamCount();
  auto* fparam = new FarListItem[size];
  memset(fparam, 0, sizeof(FarListItem) * (size));

  size_t count = 0;
  std::vector<SString> default_params = farEditorSet->defaultType->enumParams();
  for (auto& default_param : default_params) {
    fparam[count++].Text = _wcsdup(default_param.getWChars());
  }
  std::vector<SString> type_params = type->enumParams();
  for (auto& type_param : type_params) {
    if (farEditorSet->defaultType->getParamValue(type_param) == nullptr) {
      fparam[count++].Text = _wcsdup(type_param.getWChars());
    }
  }

  fparam[0].Flags = LIF_SELECTED;
  return buildFarList(fparam, count);
}

void HrcSettingsForm::ChangeParamValueListType(bool dropdownlist) const
{
  size_t s = Info.SendDlgMessage(hDlg, DM_GETDLGITEM, IDX_CH_PARAM_VALUE_LIST, nullptr);
  auto* DialogItem = static_cast<FarDialogItem*>(calloc(1, s));
  FarGetDialogItem fgdi {};
  fgdi.Item = DialogItem;
  fgdi.StructSize = sizeof(FarGetDialogItem);
  fgdi.Size = s;
  Info.SendDlgMessage(hDlg, DM_GETDLGITEM, IDX_CH_PARAM_VALUE_LIST, &fgdi);
  DialogItem->Flags = DIF_LISTWRAPMODE;
  if (dropdownlist) {
    DialogItem->Flags |= DIF_DROPDOWNLIST;
  }
  Info.SendDlgMessage(hDlg, DM_SETDLGITEM, IDX_CH_PARAM_VALUE_LIST, DialogItem);

  free(DialogItem);
}

FarList* HrcSettingsForm::buildFarList(FarListItem* list, size_t count)
{
  auto* lparam = new FarList;
  lparam->Items = list;
  lparam->ItemsNumber = count;
  lparam->StructSize = sizeof(FarList);
  return lparam;
}

void HrcSettingsForm::removeFarList(FarList* list)
{
  for (size_t idx = 0; idx < list->ItemsNumber; idx++) {
    delete[] list->Items[idx].Text;
  }
  delete[] list->Items;
  delete list;
}