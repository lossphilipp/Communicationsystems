typedef struct {
    uint8_t version;
    uint8_t type;       // pressed=1, released=0
    uint8_t button_id;  // left=1, right=2, unkknown/default=0
    // uint8_t rfu;        // reserved for future use if not packed
    uint64_t timestamp;
} __attribute__((packed)) ButtonEvent_old; // better not packed if used internally (packed only for sending)

// Bitfield: pack version and type into one
/*typedef enum { // Typ int für Werte
    EventTypeKey, 
    EventTypeLED,
    EventTypeCount = 8 // Wertebereich prüfen wenn man bei 0 anfängt (kein EventType... = 5)
} EventHeaderType_e;*/
#define EventTypeKey    0 
#define EventTypeLED    1
#define EventTypeCount  2
#if EventTypeCount > 7 // Umgebung prüfen, damit enum nicht größer als die 3 Bits ist (geht nicht mit enum ?!?)
    #error "Too many Events!"
#endif

typedef struct {
    uint8_t version : 3;
    uint8_t type    : 3; // key, led, ...
    uint8_t rfu     : 2; // if 4 -> 10 Bit: Compiler legt 6 Bit ins eines, 4 ins nächste Byte
    // could add checksum; untere Schicht
} EventHeader_t;

typedef struct {
    uint64_t timestamp;
    uint8_t type;       // pressed=1, released=0
    uint8_t button_id;  // left=1, right=2, unkknown/default=0
} ButtonEvent;

typedef struct {
    uint64_t timestamp;
} LEDEvent;

typedef struct { // auch intern verwendbar
    EventHeader_t header;
    // now could be ButtonEvent or LEDEvent or some other (XML, ...) event -> use union
    union {
        ButtonEvent button_event;
        LEDEvent led_event;
    }; // Nachteil: Speicher reserviert ist 12 Byte weil größtes Event (ButtonEvent_new) hergenommen wird
} Event;

static const char *TAG = "BUTTON_SEND";
#define VERSION     1
// static const int VERSION = 1; // kommt in Speicher -> braucht mehr Platz als define

void show_use_case_event(void) {
    Event e;
    e.header.version = VERSION;
    e.header.type = EventTypeKey;
    e.button_event.button_id = 1;
    e.button_event.type = 1;
    // e.led_event.timestamp = 0x00; // würde button_event überschreiben!
}