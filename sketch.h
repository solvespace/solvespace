
#ifndef __SKETCH_H
#define __SKETCH_H

typedef struct hRequestTag hRequest;
typedef struct hEntityTag hEntity;
typedef struct hPointTag hPoint;
typedef struct hParamTag hParam;

typedef struct hRequestTag {
    int     v;

    hEntity entity(int i);
} hRequest;

typedef struct {
    static const int REQUEST_LINE_SEGMENT                       = 0;
    static const int REQUEST_STEP_REPEAT_TRANSLATE              = 1;
    static const int REQUEST_STEP_REPEAT_TRANSLATE_SYM          = 2;
    int         type;

    hRequest    h;
} Request;

typedef struct hEntityTag {
    int     v;

    hRequest request(int i);
    hPoint point(int i);
} hEntity;

typedef struct {
    static const int ENTITY_LINE_SEGMENT                        = 0;
    static const int ENTITY_PWL_SEGMENT                         = 1;
    int         type;

    hEntity     h;
} Entity;

typedef struct hPointTag {
} hPoint;

typedef struct {
    
    hPoint      h;
} Point;

typedef struct hParamTag {
} hParam;

typedef struct {
    double      val;

    hParam      h;
} Param;

#endif

