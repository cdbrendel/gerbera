/*MT*
    
    MediaTomb - http://www.mediatomb.cc/
    
    js_functions.cc - this file is part of MediaTomb.
    
    Copyright (C) 2005 Gena Batyan <bgeradz@mediatomb.cc>,
                       Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>
    
    Copyright (C) 2006-2007 Gena Batyan <bgeradz@mediatomb.cc>,
                            Sergey 'Jin' Bostandzhyan <jin@mediatomb.cc>,
                            Leonhard Wimmer <leo@mediatomb.cc>
    
    MediaTomb is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.
    
    MediaTomb is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    version 2 along with MediaTomb; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
    
    $Id$
*/

/// \file js_functions.cc

#ifdef HAVE_CONFIG_H
    #include "autoconfig.h"
#endif

#ifdef HAVE_JS

#include "js_functions.h"

#include "script.h"
#include <typeinfo>
#include "storage.h"
#include "content_manager.h"
#include "metadata_handler.h"

using namespace zmm;

extern "C" {

JSBool 
js_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i;
    JSString *str;

    for (i = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        log_js("%s\n", JS_GetStringBytes(str));
    }
    return JS_TRUE;
}

JSBool
js_copyObject(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval arg;
    JSObject *js_cds_obj;
    JSObject *js_cds_clone_obj;

    Script *self = (Script *)JS_GetPrivate(cx, obj);

    try
    {
        arg = argv[0];
        if (!JSVAL_IS_OBJECT(arg))
            return JS_FALSE;
        if (!JS_ValueToObject(cx, arg, &js_cds_obj))
            return JS_FALSE;

        Ref<CdsObject> cds_obj = self->jsObject2cdsObject(js_cds_obj);
        js_cds_clone_obj = JS_NewObject(cx, NULL, NULL, NULL);
        self->cdsObject2jsObject(cds_obj, js_cds_clone_obj);

        *rval = OBJECT_TO_JSVAL(js_cds_clone_obj);

        return JS_TRUE;

    }
    catch (Exception e)
    {
        e.printStackTrace();
    }
    return JS_FALSE;
}

JSBool
js_addCdsObject(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    try
    {
        jsval arg;
        JSString *str;
        String path;
        String containerclass;

        JSObject *js_cds_obj;
        JSObject *js_orig_obj;
        Ref<CdsObject> orig_object;

        Script *self = (Script *)JS_GetPrivate(cx, obj);

        if (self == NULL)
        {
            log_debug("Could not retrieve class instance from global object\n");
            return FALSE;
        }

        arg = argv[0];
        if (!JSVAL_IS_OBJECT(arg))
            return JS_FALSE;
        if (!JS_ValueToObject(cx, arg, &js_cds_obj))
            return JS_FALSE;

        str = JS_ValueToString(cx, argv[1]);
        if (!str)
            path = _("/");
        else
            path = String(JS_GetStringBytes(str));

        JSString *cont = JS_ValueToString(cx, argv[2]);
        if (cont)
        {
            containerclass = String(JS_GetStringBytes(cont));
            if (!string_ok(containerclass) || containerclass == "undefined")
                containerclass = nil;
        }

        if (self->whoami() == S_PLAYLIST)
            js_orig_obj = self->getObjectProperty(obj, _("playlist"));
        else if (self->whoami() == S_IMPORT)
            js_orig_obj = self->getObjectProperty(obj, _("orig"));
        
        if (js_orig_obj == NULL)
        {
            log_debug("Could not retrieve orig/playlist object\n");
            return JS_FALSE;
        }

        orig_object = self->jsObject2cdsObject(js_orig_obj);
        if (orig_object == nil)
            return JS_FALSE;

        Ref<CdsObject> cds_obj = self->jsObject2cdsObject(js_cds_obj);
        if (cds_obj == nil)
            return JS_FALSE;

        Ref<ContentManager> cm = ContentManager::getInstance();

        int id;
        
        if (self->whoami() == S_PLAYLIST)
            id = cm->addContainerChain(path, containerclass, 
                    orig_object->getID());
        else
            id = cm->addContainerChain(path, containerclass);

        cds_obj->setParentID(id);
        if (!IS_CDS_ITEM_EXTERNAL_URL(cds_obj->getObjectType()) &&
            !IS_CDS_ITEM_INTERNAL_URL(cds_obj->getObjectType()))
        {
            /// \todo get hidden file setting from config manager?
            /// what about same stuff in content manager, why is it not used
            /// there?

            if (self->whoami() == S_PLAYLIST)
            {
                int pcd_id = cm->addFile(cds_obj->getLocation(), false, false, true);
                if (pcd_id == INVALID_OBJECT_ID)
                    return JS_FALSE;

                cds_obj->setRefID(pcd_id);
            }
            else
                cds_obj->setRefID(orig_object->getID());

            cds_obj->setFlag(OBJECT_FLAG_USE_RESOURCE_REF);
        }
        else if (IS_CDS_ITEM_EXTERNAL_URL(cds_obj->getObjectType()) || 
                 IS_CDS_ITEM_INTERNAL_URL(cds_obj->getObjectType()))
        {
            if (self->whoami() == S_PLAYLIST)
            {
                cds_obj->setFlag(OBJECT_FLAG_PLAYLIST_REF);
                cds_obj->setRefID(orig_object->getID());
            }
        }

        cds_obj->setID(INVALID_OBJECT_ID);
        cm->addObject(cds_obj);

        /* setting object ID as return value */
        String tmp = String::from(id);

        JSString *str2 = JS_NewStringCopyN(cx, tmp.c_str(), tmp.length());
        if (!str2)
            return JS_FALSE;
        *rval = STRING_TO_JSVAL(str2);

        return JS_TRUE;
    }
    catch (Exception e)
    {
        e.printStackTrace();
    }
    return JS_FALSE;
}

} // extern "C"

#endif //HAVE_JS