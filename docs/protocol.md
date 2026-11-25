# Set log format

One logged set, as the board prints it today over Serial:

```
LOG,<exercise>,<set number>,<weight lb>,<reps>,<millis>
```

Example: `LOG,Pushdown,1,80,8,12345`

| Field | Meaning |
| --- | --- |
| exercise | Name from the board's exercise list |
| set number | Set count for that exercise in this session |
| weight lb | Weight in pounds |
| reps | Reps in the set |
| millis | Milliseconds since the board started (not a real clock) |

## To decide for the Bluetooth link

- Service and characteristic UUIDs (generate our own; none chosen yet).
- Whether the phone adds the real timestamp when it receives each set.
- How the board stores unsent sets and resends after a reconnect.
