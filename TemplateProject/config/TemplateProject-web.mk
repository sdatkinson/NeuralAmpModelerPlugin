# IPLUG2_ROOT should point to the top level IPLUG2 folder from the project folder
# By default, that is three directories up from /Examples/TemplateProject/config
IPLUG2_ROOT = ../../iPlug2

include ../../common-web.mk

SRC += $(PROJECT_ROOT)/TemplateProject.cpp

# WAM_SRC += 

# WAM_CFLAGS +=

WEB_CFLAGS += -DIGRAPHICS_NANOVG -DIGRAPHICS_GLES2

WAM_LDFLAGS += -O0 -s EXPORT_NAME="'AudioWorkletGlobalScope.WAM.TemplateProject'" -s ASSERTIONS=0

WEB_LDFLAGS += -O0 -s ASSERTIONS=0

WEB_LDFLAGS += $(NANOVG_LDFLAGS)
