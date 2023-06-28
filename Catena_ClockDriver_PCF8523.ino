/*

Module: Catena_ClockDriver_PCF8523.ino

Function:
    This is an example sketch for PCF8523 Clock Driver library.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Pranau R, MCCI Corporation   June 2023

*/

// Date and time functions using a PCF8523 RTC connected via I2C and Wire lib
#include <Catena4430_cClockDriver_PCF8523.h>
#include <Catena4430.h>
#include <Catena_Date.h>
#include <Arduino.h>
#include <Catena.h>
#include <Catena_CommandStream.h>

using namespace McciCatena4430;
using namespace McciCatena;

McciCatena::cCommandStream::CommandFn cmdDate;

cClockDriver_PCF8523 gRtc { &Wire };
cDate d;
Catena gCatena;

int8_t OFFSET = 46;

/****************************************************************************\
|
|   User commands
|
\****************************************************************************/

// the individual commmands are put in this table
static const cCommandStream::cEntry sMyExtraCommmands[] =
    {
    { "date", cmdDate },
    // other commands go here....
    };

/* a top-level structure wraps the above and connects to the system table */
/* it optionally includes a "first word" so you can for sure avoid name clashes */
static cCommandStream::cDispatch
sMyExtraCommands_top(
    sMyExtraCommmands,          /* this is the pointer to the table */
    sizeof(sMyExtraCommmands),  /* this is the size of the table */
    nullptr                     /* this is no "first word" for all the commands in this table */
    );

void setup ()
    {
    gCatena.begin();
    // Serial.begin(57600);

    while (!Serial); // wait for serial port to connect. Needed for native USB

    /* add our application-specific commands */
    gCatena.addCommands(
        /* name of app dispatch table, passed by reference */
        sMyExtraCommands_top,
        /*
        || optionally a context pointer using static_cast<void *>().
        || normally only libraries (needing to be reentrant) need
        || to use the context pointer.
        */
        nullptr
        );

    if (! gRtc.begin())
        {
        gCatena.SafePrintf("RTC failed to intialize\n");
        }

    if (gRtc.offsetAdjustment(gRtc.kRegOffsetMode::TwoHours, OFFSET))
        {
        gCatena.SafePrintf("RTC OFFSET adjusment done!\n");
        gCatena.SafePrintf("RTC OFFSET set: %d \n", OFFSET);
        }
    else
        {
        gCatena.SafePrintf("RTC OFFSET adjusment failed!\n");        
        }
    }

void loop ()
    {
    gCatena.poll();
    if (! gRtc.get(d))
        gCatena.SafePrintf("RTC is not running\n");
    else
        {
        gCatena.SafePrintf("RTC is running. Date: %d-%02d-%02d %02d:%02d:%02dZ\n",
            d.year(), d.month(), d.day(),
            d.hour(), d.minute(), d.second()
            );
        }
    delay(3000);
    }
/*

Name:   ::cmdDate()

Function:
    Command dispatcher for "date" command.

Definition:
    McciCatena::cCommandStream::CommandFn cmdDate;

    McciCatena::cCommandStream::CommandStatus cmdDate(
        cCommandStream *pThis,
        void *pContext,
        int argc,
        char **argv
        );

Description:
    The "date" command has the following syntax:

    date
        Display the current date.

    date yyyy-mm-dd hh:mm:ss
        Set the date and time.

    date hh:mm:ss
        Set the time only (leave date alone)

    date yyy-mm-dd
        Set the date only (leave time alone)

    The clock runs in the UTC/GMT/Z time-zone.

Returns:
    cCommandStream::CommandStatus::kSuccess if successful.
    Some other value for failure.

*/

// argv[0] is the matched command name.
// argv[1], [2] if present are the new mask date, time.
cCommandStream::CommandStatus cmdDate(
    cCommandStream *pThis,
    void *pContext,
    int argc,
    char **argv
    )
    {
    bool fResult;
    cDate d;
    bool fInitialized = gRtc.isInitialized();
    const char *pEnd;

    if (fInitialized)
        {
        unsigned errCode = 0;
        if (! gRtc.get(d, &errCode))
            {
            fInitialized = false;
            pThis->printf("gRtc.get() failed: %u\n", errCode);
            }
        }

    if (argc < 2)
        {
        if (! fInitialized)
            pThis->printf("clock not initialized\n");
        else
            pThis->printf("%04u-%02u-%02u %02u:%02u:%02uZ (GPS: %lu)\n",
                d.year(), d.month(), d.day(),
                d.hour(), d.minute(), d.second(),
                static_cast<unsigned long>(d.getGpsTime())
                );

        return cCommandStream::CommandStatus::kSuccess;
        }

    fResult = true;
    if (argc > 3)
        {
        fResult = false;
        pThis->printf("too many args\n");
        }

    if (fResult)
        {
        if (d.parseDateIso8601(argv[1]))
            {
            if (argv[2] != nullptr && ! d.parseTime(argv[2], &pEnd))
                {
                pThis->printf("invalid time after date: %s\n", argv[2]);
                fResult = false;
                }
            else if (argv[2] != nullptr)
                {
                if (pEnd == NULL ||
                    ! ((pEnd[0] == 'z' || pEnd[0] == 'Z') && pEnd[1] == '\0'))
                    {
                    pThis->printf("expected Z suffix to remind you it's UTC+0 (GMT) time\n");
                    fResult = false;
                    }
                }
            }
        else if (d.parseTime(argv[1], &pEnd))
            {
            if (argv[2] != nullptr);
                {
                pThis->printf("nothing allowed after time: %s\n", argv[2]);
                fResult = false;
                }
            if (pEnd == NULL ||
                ! ((pEnd[0] == 'z' || pEnd[0] == 'Z') && pEnd[1] == '\0'))
                {
                pThis->printf("expected Z suffix to remind you it's UTC+0 (GMT) time\n");
                fResult = false;
                }
            }
        else
            {
            pThis->printf("not a date or time: %s\n", argv[1]);
            fResult = false;
            }

        if (fResult)
            {
            unsigned errCode;
            if (! gRtc.set(d, &errCode))
                {
                pThis->printf("couldn't set clock: %u\n", errCode);
                return cCommandStream::CommandStatus::kIoError;
                }
            }
        }

    return fResult ? cCommandStream::CommandStatus::kSuccess
                   : cCommandStream::CommandStatus::kInvalidParameter
                   ;
    }
