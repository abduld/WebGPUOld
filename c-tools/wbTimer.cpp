

#include	<wb.h>
#include	<time.h>

#ifdef _WIN32
uint64_t _hrtime_frequency = 0;
#endif /* _WIN32 */
wbTimer_t _timer = NULL;

#ifdef __APPLE__
static double o_timebase = 0;
static uint64_t o_timestart = 0;
#endif /* __APPLE__ */

uint64_t _hrtime(void) {
#define NANOSEC ((uint64_t) 1e9)
#ifdef _MSC_VER
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter)) {
        return 0;
    }
    return ((uint64_t) counter.LowPart * NANOSEC / _hrtime_frequency) +
            (((uint64_t) counter.HighPart * NANOSEC / _hrtime_frequency) << 32);
#else
    struct timespec ts;
#ifdef __APPLE__
#define O_NANOSEC   (+1.0E-9)
#define O_GIGA      UINT64_C(1000000000)
    if (!o_timestart) {
        mach_timebase_info_data_t tb = { 0 };
        mach_timebase_info(&tb);
        o_timebase = tb.numer;
        o_timebase /= tb.denom;
        o_timestart = mach_absolute_time();
    }
    double diff = (mach_absolute_time() - o_timestart) * o_timebase;
    ts.tv_sec = diff * O_NANOSEC;
    ts.tv_nsec = diff - (ts.tv_sec * O_GIGA);
#undef O_NANOSEC
#undef O_GIGA
#else /* __APPLE__ */
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif /* __APPLE__ */
    return (((uint64_t) ts.tv_sec) * NANOSEC + ts.tv_nsec);
#endif /* _MSC_VER */
#undef NANOSEC
}


static inline wbTimerNode_t wbTimerNode_new(int id, wbTimerKind_t kind, const char * file, const char * fun, int startLine) {
    wbTimerNode_t node = wbNew(struct st_wbTimerNode_t);
    wbTimerNode_setId(node, id);      
    wbTimerNode_setLevel(node, 0);      
    wbTimerNode_setStoppedQ(node, wbFalse);
    wbTimerNode_setKind(node, kind);
    wbTimerNode_setStartTime(node, 0);
    wbTimerNode_setEndTime(node, 0);   
    wbTimerNode_setElapsedTime(node, 0);
    wbTimerNode_setStartLine(node, startLine);
    wbTimerNode_setEndLine(node, 0);
    wbTimerNode_setStartFunction(node, fun);
    wbTimerNode_setEndFunction(node, NULL);
    wbTimerNode_setStartFile(node, file);
    wbTimerNode_setEndFile(node, NULL);
    wbTimerNode_setNext(node, NULL);
    wbTimerNode_setPrevious(node, NULL);
    wbTimerNode_setParent(node, NULL);
    wbTimerNode_setMessage(node, NULL);
    return node;
}

static inline void wbTimerNode_delete(wbTimerNode_t node) {
    if (node != NULL) {
        if (wbTimerNode_getMessage(node)) {
            wbDelete(wbTimerNode_getMessage(node));
        }
        wbDelete(node);
    }
}

static inline const char * _nodeKind(wbTimerKind_t kind) {
    switch (kind) {
        case wbTimerKind_Generic:
            return "Generic";
        case wbTimerKind_IO:
            return "IO";
        case wbTimerKind_GPU:
            return "GPU";
        case wbTimerKind_Copy:
            return "Copy";
        case wbTimerKind_Driver:
            return "Driver";
        case wbTimerKind_CopyAsync:
            return "CopyAsync";
        case wbTimerKind_Compute:
            return "Compute";
        case wbTimerKind_CPUGPUOverlap:
            return "CPUGPUOverlap";
    }
    return "Undefined";
}


#ifdef WB_USE_JSON
static inline json_t * wbTimerNode_toJSON(wbTimerNode_t node) {
    if (node == NULL) {
        return NULL;
    } else {
        json_t * obj = json_object();

        json_t * type = json_string("TimerNode");
        json_t * id = json_integer(wbTimerNode_getId(node));
        json_t * stoppedQ = wbTimerNode_stoppedQ(node) ? json_true() : json_false();
        json_t * kind = json_string(_nodeKind(wbTimerNode_getKind(node)));
        json_t * startTime = json_integer(wbTimerNode_getStartTime(node));
        json_t * endTime = json_integer(wbTimerNode_getEndTime(node));
        json_t * elapsedTime = json_integer(wbTimerNode_getElapsedTime(node));
        json_t * startLine = json_integer(wbTimerNode_getStartLine(node));
        json_t * endLine = json_integer(wbTimerNode_getEndLine(node));
        json_t * startFunction = json_string(wbTimerNode_getStartFunction(node));
        json_t * endFunction = json_string(wbTimerNode_getEndFunction(node));
        json_t * startFile = json_string(wbTimerNode_getStartFile(node));
        json_t * endFile = json_string(wbTimerNode_getEndFile(node));
        json_t * parentId = wbTimerNode_hasParent(node) ?
								json_integer(wbTimerNode_getId(wbTimerNode_getParent(node))):
                                json_integer(-1);
        json_t * msg = json_string(wbTimerNode_getMessage(node));

        json_object_set(obj, "Type", type);
        json_object_set(obj, "Id", id);
        json_object_set(obj, "StoppedQ", stoppedQ);
        json_object_set(obj, "Kind", kind);
        json_object_set(obj, "StartTime", startTime);
        json_object_set(obj, "EndTime", endTime);
        json_object_set(obj, "ElapsedTime", elapsedTime);
        json_object_set(obj, "StartLine", startLine);
        json_object_set(obj, "EndLine", endLine);
        json_object_set(obj, "StartFunction", startFunction);
        json_object_set(obj, "EndFunction", endFunction);
        json_object_set(obj, "StartFile", startFile);
        json_object_set(obj, "EndFile", endFile);
        json_object_set(obj, "ParentId", parentId);
        json_object_set(obj, "Message", msg);

        return obj;
    }
}
#endif /* WB_USE_JSON */

static inline string wbTimerNode_toXML(wbTimerNode_t node) {
	if (node == NULL) {
        return "";
    } else {
		stringstream ss;

		ss << "<node>\n";
			ss << "<type>" << "TimerNode" << "</type>\n";
			ss << "<id>" << wbTimerNode_getId(node) << "</id>\n";
			ss << "<stoppedQ>" <<  wbString(wbTimerNode_stoppedQ(node) ? "true" : "false") << "</stoppedQ>\n";
			ss << "<kind>" << _nodeKind(wbTimerNode_getKind(node)) << "</kind>\n";
			ss << "<start_time>" << wbTimerNode_getStartTime(node) << "</start_time>\n";
			ss << "<end_time>" << wbTimerNode_getEndTime(node) << "</end_time>\n";
			ss << "<elapsed_time>" << wbTimerNode_getElapsedTime(node) << "</elapsed_time>\n";
			ss << "<start_line>" << wbTimerNode_getStartLine(node) << "</start_line>\n";
			ss << "<end_line>" << wbTimerNode_getEndLine(node) << "</end_line>\n";
			ss << "<start_function>" << wbTimerNode_getStartFunction(node) << "</start_function>\n";
			ss << "<end_function>" << wbTimerNode_getEndFunction(node) << "</end_function>\n";
			ss << "<start_file>" << wbTimerNode_getStartFile(node) << "</start_file>\n";
			ss << "<end_file>" << wbTimerNode_getEndFile(node) << "</end_file>\n";
			ss << "<parent_id>" << wbString(wbTimerNode_hasParent(node) ? wbTimerNode_getId(wbTimerNode_getParent(node)) : -1) << "</parent_id>\n";
			ss << "<message>" << wbTimerNode_getMessage(node) << "</message>\n";
		ss << "</node>\n";

        return ss.str();
    }
}

#define wbTimer_getLength(timer)                ((timer)->length)
#define wbTimer_getHead(timer)                  ((timer)->head)
#define wbTimer_getTail(timer)                  ((timer)->tail)
#define wbTimer_getStartTime(timer)             ((timer)->startTime)
#define wbTimer_getEndTime(timer)               ((timer)->endTime)
#define wbTimer_getElapsedTime(timer)           ((timer)->elapsedTime)

#define wbTimer_setLength(timer, val)           (wbTimer_getLength(timer) = val)
#define wbTimer_setHead(timer, val)             (wbTimer_getHead(timer) = val)
#define wbTimer_setTail(timer, val)             (wbTimer_getTail(timer) = val)
#define wbTimer_setStartTime(node, val)         (wbTimer_getStartTime(node) = val)
#define wbTimer_setEndTime(node, val)           (wbTimer_getEndTime(node) = val)
#define wbTimer_setElapsedTime(node, val)       (wbTimer_getElapsedTime(node) = val)

#define wbTimer_incrementLength(timer)          (wbTimer_getLength(timer)++)
#define wbTimer_decrementLength(timer)          (wbTimer_getLength(timer)--)

#define wbTimer_emptyQ(timer)                   (wbTimer_getLength(timer) == 0)

void wbTimer_delete(wbTimer_t timer) {
    if (timer != NULL) {
        wbTimerNode_t tmp, iter;

        iter = wbTimer_getHead(timer);
        while (iter) {
            tmp = wbTimerNode_getNext(iter);
            wbTimerNode_delete(iter);
            iter = tmp;
        }

        wbDelete(timer);
    }
}

#ifdef WB_USE_JSON
json_t * wbTimer_toJSON(wbTimer_t timer) {
    if (timer == NULL) {
        return NULL;
    } else {
        json_t * obj;
        json_t * type;
        json_t * elems;
        json_t * startTime;
        json_t * endTime;
        json_t * elapsedTime;
        wbTimerNode_t iter;
		uint64_t currentTime;

		currentTime = _hrtime();
		
		wbTimer_setEndTime(timer, currentTime);
		wbTimer_setElapsedTime(timer, currentTime - wbTimer_getStartTime(timer));

        obj = json_object();
        type = json_string("Timer");
        startTime = json_integer(wbTimer_getStartTime(timer));
        endTime = json_integer(wbTimer_getEndTime(timer));
        elapsedTime = json_integer(wbTimer_getElapsedTime(timer));
        elems = json_array();


        for (iter = wbTimer_getHead(timer); iter != NULL; iter = wbTimerNode_getNext(iter)) {
            json_t * elem;
            if (!wbTimerNode_stoppedQ(iter)) {
                wbTimerNode_setEndTime(iter, currentTime);
                wbTimerNode_setElapsedTime(iter, currentTime - wbTimerNode_getStartTime(iter));
            }
            elem = wbTimerNode_toJSON(iter);
            json_array_append(elems, elem);
        }

        json_object_set(obj, "Type", type);
        json_object_set(obj, "Elements", elems);
        json_object_set(obj, "StartTime", startTime);
        json_object_set(obj, "EndTime", endTime);
        json_object_set(obj, "ElapsedTime", elapsedTime);

        return obj;
    }
}

json_t * wbTimer_toJSON() {
	return wbTimer_toJSON(_timer);
}
#endif /* WB_USE_JSON */

string wbTimer_toXML(wbTimer_t timer) {
	if (timer == NULL) {
        return "";
    } else {
		stringstream ss;
        wbTimerNode_t iter;
		uint64_t currentTime;
		
		currentTime = _hrtime();
		
		wbTimer_setEndTime(timer, currentTime);
		wbTimer_setElapsedTime(timer, currentTime - wbTimer_getStartTime(timer));

		ss << "<timer>\n";
			ss << "<type>" << "Timer" << "</type>\n";
			ss << "<start_time>" << wbTimer_getStartTime(timer) << "</start_time>\n";
			ss << "<end_time>" << wbTimer_getEndTime(timer) << "</end_time>\n";
			ss << "<elapsed_time>" << wbTimer_getElapsedTime(timer) << "</elapsed_time>\n";
			ss << "<nodes>\n";
				for (iter = wbTimer_getHead(timer); iter != NULL; iter = wbTimerNode_getNext(iter)) {
					if (!wbTimerNode_stoppedQ(iter)) {
						wbTimerNode_setEndTime(iter, currentTime);
						wbTimerNode_setElapsedTime(iter, currentTime - wbTimerNode_getStartTime(iter));
					}
					ss << wbTimerNode_toXML(iter);
				}
			ss << "</nodes>\n";
		ss << "</timer>\n";

        return ss.str();
    }
}

string wbTimer_toXML() {
	return wbTimer_toXML(_timer);
}

wbTimer_t wbTimer_new(void) {
    wbTimer_t timer = wbNew(struct st_wbTimer_t);
    wbTimer_setLength(timer, 0);
    wbTimer_setHead(timer, NULL);
    wbTimer_setTail(timer, NULL);
    wbTimer_setStartTime(timer, _hrtime());
    wbTimer_setEndTime(timer, 0);
    wbTimer_setElapsedTime(timer, 0);

    return timer;
}

static inline wbTimerNode_t _findParent(wbTimer_t timer) {
    wbTimerNode_t iter;

    for (iter = wbTimer_getTail(timer); iter != NULL; iter = wbTimerNode_getPrevious(iter)) {
        if (!wbTimerNode_stoppedQ(iter)) {
            return iter;
        }
    }
    return NULL;
}

static inline void _insertIntoList(wbTimer_t timer, wbTimerNode_t node) {
    if (wbTimer_emptyQ(timer)) {
        wbTimer_setHead(timer, node);
        wbTimer_setTail(timer, node);
    } else {
        wbTimerNode_t end = wbTimer_getTail(timer);
        wbTimer_setTail(timer, node);
        wbTimerNode_setNext(end, node);
        wbTimerNode_setPrevious(node, end);
    }
    wbTimer_incrementLength(timer);
}

wbTimerNode_t wbTimer_start(wbTimerKind_t kind, const char * file, const char * fun, int line) {
    int id;
    uint64_t currentTime;
    wbTimerNode_t node;
    wbTimerNode_t parent;
    
    wb_init();

    currentTime = _hrtime();
    
    id = wbTimer_getLength(_timer);
    
    node = wbTimerNode_new(id, kind, file, fun, line);

    parent = _findParent(_timer);
    _insertIntoList(_timer, node);

	wbTimerNode_setStartTime(node, currentTime);
    wbTimerNode_setParent(node, parent);
    if (parent != NULL) {
        wbTimerNode_setLevel(node, wbTimerNode_getLevel(parent)+1);
    }

    return node;
}

wbTimerNode_t wbTimer_start(wbTimerKind_t kind, string msg, const char * file, const char * fun, int line) {
    wbTimerNode_t node = wbTimer_start(kind, fun, file, line);
    wbTimerNode_setMessage(node, wbString_duplicate(msg));
    return node;
}


static inline wbTimerNode_t _findNode(wbTimer_t timer, wbTimerKind_t kind, string msg) {
    wbTimerNode_t iter;

    for (iter = wbTimer_getTail(timer); iter != NULL; iter = wbTimerNode_getPrevious(iter)) {
		if (msg == "") {
			if (!wbTimerNode_stoppedQ(iter) && wbTimerNode_getKind(iter) == kind) {
				return iter;
			}
		} else {
			if (!wbTimerNode_stoppedQ(iter) && wbTimerNode_getKind(iter) == kind && msg == wbTimerNode_getMessage(iter)) {
				return iter;
			}
		}
    }
    return NULL;
}

void wbTimer_stop(wbTimerKind_t kind, string msg, const char * file, const char * fun, int line) {
    uint64_t currentTime;
    wbTimerNode_t node;
    
    currentTime = _hrtime();

    node = _findNode(_timer, kind, msg);

    wbAssert(node != NULL);
    if (node == NULL) {
        return ;
    }

    wbTimerNode_setEndTime(node, currentTime);
    wbTimerNode_setElapsedTime(node, currentTime - wbTimerNode_getStartTime(node));
    wbTimerNode_setEndLine(node, line);
    wbTimerNode_setEndFunction(node, fun);
    wbTimerNode_setEndFile(node, file);
    wbTimerNode_setStoppedQ(node, wbTrue);

    return ;
}


void wbTimer_stop(wbTimerKind_t kind, const char * file, const char * fun, int line) {
	wbTimer_stop(kind, "", file, fun, line);
}