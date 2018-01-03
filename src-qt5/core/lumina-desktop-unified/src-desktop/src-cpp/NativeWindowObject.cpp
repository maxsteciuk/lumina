//===========================================
//  Lumina-DE source code
//  Copyright (c) 2017-2018, Ken Moore
//  Available under the 3-clause BSD license
//  See the LICENSE file for full details
//===========================================
#include "NativeWindowObject.h"
#include <QQmlEngine>
#include <QDebug>

// == QML Type Registration ==
void NativeWindowObject::RegisterType(){
  static bool done = false;
  if(done){ return; }
  done=true;
  qmlRegisterType<NativeWindowObject>("Lumina.Backend.NativeWindowObject",2,0, "NativeWindowObject");
}

// === PUBLIC ===
NativeWindowObject::NativeWindowObject(WId id) : QObject(){
  winid = id;
  frameid = 0;
  dmgID = 0;
}

NativeWindowObject::~NativeWindowObject(){
  hash.clear();
}

void NativeWindowObject::addFrameWinID(WId fid){
  frameid = fid;
}

void NativeWindowObject::addDamageID(unsigned int dmg){
  dmgID = dmg;
}

bool NativeWindowObject::isRelatedTo(WId tmp){
  return (relatedTo.contains(tmp) || winid == tmp || frameid == tmp);
}

WId NativeWindowObject::id(){
  return winid;
}

WId NativeWindowObject::frameId(){
  return frameid;
}

unsigned int NativeWindowObject::damageId(){
  return dmgID;
}

QVariant NativeWindowObject::property(NativeWindowObject::Property prop){
  if(hash.contains(prop)){ return hash.value(prop); }
  else if(prop == NativeWindowObject::RelatedWindows){ return QVariant::fromValue(relatedTo); }
  return QVariant(); //null variant
}

void NativeWindowObject::setProperty(NativeWindowObject::Property prop, QVariant val, bool force){
  if(prop == NativeWindowObject::RelatedWindows){ relatedTo = val.value< QList<WId> >(); }
  else if(prop == NativeWindowObject::None || (!force && hash.value(prop)==val)){ return; }
  else{ hash.insert(prop, val); }
  emitSinglePropChanged(prop);
  emit PropertiesChanged(QList<NativeWindowObject::Property>() << prop, QList<QVariant>() << val);
}

void NativeWindowObject::setProperties(QList<NativeWindowObject::Property> props, QList<QVariant> vals, bool force){
  for(int i=0; i<props.length(); i++){
    if(i>=vals.length()){ props.removeAt(i); i--; continue; } //no corresponding value for this property
    if(props[i] == NativeWindowObject::None || (!force && (hash.value(props[i]) == vals[i])) ){ props.removeAt(i); vals.removeAt(i); i--; continue; } //Invalid property or identical value
    hash.insert(props[i], vals[i]);
    emitSinglePropChanged(props[i]);
  }
  emit PropertiesChanged(props, vals);
}

void NativeWindowObject::requestProperty(NativeWindowObject::Property prop, QVariant val, bool force){
  if(prop == NativeWindowObject::None || prop == NativeWindowObject::RelatedWindows || (!force && hash.value(prop)==val) ){ return; }
  emit RequestPropertiesChange(winid, QList<NativeWindowObject::Property>() << prop, QList<QVariant>() << val);
}

void NativeWindowObject::requestProperties(QList<NativeWindowObject::Property> props, QList<QVariant> vals, bool force){
  //Verify/adjust inputs as needed
  for(int i=0; i<props.length(); i++){
    if(i>=vals.length()){ props.removeAt(i); i--; continue; } //no corresponding value for this property
    if(props[i] == NativeWindowObject::None || props[i] == NativeWindowObject::RelatedWindows || (!force && hash.value(props[i])==vals[i]) ){ props.removeAt(i); vals.removeAt(i); i--; continue; } //Invalid property or identical value
    /*if( (props[i] == NativeWindowObject::Visible || props[i] == NativeWindowObject::Active) && frameid !=0){
      //These particular properties needs to change the frame - not the window itself
      emit RequestPropertiesChange(frameid, QList<NativeWindowObject::Property>() << props[i], QList<QVariant>() << vals[i]);
      props.removeAt(i); vals.removeAt(i); i--;
    }*/
  }
  emit RequestPropertiesChange(winid, props, vals);
}

QRect NativeWindowObject::geometry(){
  //Calculate the "full" geometry of the window + frame (if any)
  //Check that the size is between the min/max limitations
  QSize size = hash.value(NativeWindowObject::Size).toSize();
  QSize min = hash.value(NativeWindowObject::MinSize).toSize();
  QSize max = hash.value(NativeWindowObject::MaxSize).toSize();
  if(min.isValid() && min.width() > size.width() ){ size.setWidth(min.width()); }
  if(min.isValid() && min.height() > size.height()){ size.setHeight(min.height()); }
  if(max.isValid() && max.width() < size.width()  && max.width()>min.width()){ size.setWidth(max.width()); }
  if(max.isValid() && max.height() < size.height()  && max.height()>min.height()){ size.setHeight(max.height()); }
  //Assemble the full geometry
  QRect geom( hash.value(NativeWindowObject::GlobalPos).toPoint(), size );
  //Now adjust the window geom by the frame margins
  QList<int> frame = hash.value(NativeWindowObject::FrameExtents).value< QList<int> >(); //Left,Right,Top,Bottom
  //qDebug() << "Calculate Geometry:" << geom << frame;
  if(frame.length()==4){
    geom = geom.adjusted( -frame[0], -frame[2], frame[1], frame[3] );
  }
  //qDebug() << " - Total:" << geom;
  return geom;
}

// QML ACCESS FUNCTIONS (shortcuts for particular properties in a format QML can use)
QString NativeWindowObject::name(){
  return this->property(NativeWindowObject::Name).toString();
}

QString NativeWindowObject::title(){
  return this->property(NativeWindowObject::Title).toString();
}

QString NativeWindowObject::shortTitle(){
  return this->property(NativeWindowObject::ShortTitle).toString();
}

QIcon NativeWindowObject::icon(){
  return this->property(NativeWindowObject::Name).value<QIcon>();
}

bool NativeWindowObject::isSticky(){
  return (this->property(NativeWindowObject::Workspace).toInt()<0 || this->property(NativeWindowObject::States).value<QList<NativeWindowObject::State> >().contains(NativeWindowObject::S_STICKY) );
}

// ==== PUBLIC SLOTS ===
void NativeWindowObject::toggleVisibility(){
  setProperty(NativeWindowObject::Visible, !property(NativeWindowObject::Visible).toBool() );
}

void NativeWindowObject::requestClose(){
  emit RequestClose(winid);
}

void NativeWindowObject::requestKill(){
  emit RequestKill(winid);
}

void NativeWindowObject::requestPing(){
  emit RequestPing(winid);
}

// ==== PRIVATE ====
void NativeWindowObject::emitSinglePropChanged(NativeWindowObject::Property prop){
  //Simple switch to emit the QML-usable signals as properties are changed
  switch(prop){
	case NativeWindowObject::Name:
		emit nameChanged(); break;
	case NativeWindowObject::Title:
		emit titleChanged(); break;
	case NativeWindowObject::ShortTitle:
		emit shortTitleChanged(); break;
	case NativeWindowObject::Icon:
		emit iconChanged(); break;
	case NativeWindowObject::Workspace:
	case NativeWindowObject::States:
		emit stickyChanged(); break;
	default:
		break; //do nothing otherwise
  }
}
