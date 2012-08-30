#ifndef HONEYDPROFILEBINDING_H
#define HONEYDPROFILEBINDING_H

#include <node.h>
#include <v8.h>
#include "v8Helper.h"
#include "HoneydConfiguration/HoneydConfigTypes.h"

class HoneydProfileBinding : public node::ObjectWrap {
 public:
  static void Init(v8::Handle<v8::Object> target);
  
  Nova::NodeProfile * GetChild();
  static v8::Handle<v8::Value> SetVendors(const v8::Arguments& args);

private:
	HoneydProfileBinding();
	~HoneydProfileBinding();

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Save(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddPort(const v8::Arguments& args);

  Nova::NodeProfile *m_pfile;
};

#endif