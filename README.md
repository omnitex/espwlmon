# espwlmon

Tool for reading and monitoring the status of [Wear Leveling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/wear-levelling.html) layer in flash memory of ESP32 chips.

Interest is taken mainly in per sector erase count or its estimate. As program-erase (PE) memory cell lifetime is finite, knowledge of wear induced by an application is important for assessing expected memory lifetime and reliability.

This tool is aimed at long running applications utilizing the wear-levelling layer. Regular snapshots (TODO) can be captured and used for predicting the longevity of flash memory.

The tool is split to `wlmon` embedded C/C++ BE and (TODO) Python FE.

## `wlmon`
Embedded part of the project. Reads and reconstructs info about WL layer from structures and records kept in flash by WL. Currently works only with the ESP-IDF implementation of WL. That should include `safe` and `performance` modes as they only differ in runtime behavior. This implementation has a fatal flaw of not keeping any long term stats, thus needs extending and improving.


## WIP: `WL_Advanced`

Concept for extending ESP-IDF implementation of WL. Uses redundancy in the records kept and allows for per sector erase count tracking, albeit still with a resolution; only every `Nth` (`N` typically 16) erase is logged.

### `pos update record`
Written to flash on every `pos` increment == every `dummy block` move. Minimal size of `16B` required by possible flash encryption. These records get written subsequently after each other. Once `pos` reaches its maximum value, `pos` is reset, `move_count` incremented, state structure rewritten and all these records erased.

Instead of currently just 4 CRCs, store useful info in them. Mainly `sector number` of **actual physical sector** whose erase triggered the writing of `pos update record`.

| 0 | 1 | 2 | 3 |
|:-----:|:-----:|:-----:|:-----:|
| device_id | pos | sector number | CRC32 |

Before these records are erased (on conditions described above), an aggregation of which sectors are present and their frequency in these records is performed. These `erase counts` are added to a all-time-total that is kept in a buffer.

This `erase_count_buffer` stores `uint16_t` for each wear-levelled sector. As every increment in this buffer means given sector had a record and the records are written every `updaterate` ("`N`", typically 16) erases, value stored in buffer multiplied by `updaterate` gives a close estimate of sector erase count.

### `erase_count_record`

Once this aggregation is performed, non-zero erase counts are saved to additional allocated sectors for private use by WL in the following format. Again, all sector numbers and erase counts are `uint16_t`. This forms a `16B` record also.

| `sector0` | `erase_count0` | `sector1` | `erase_count1` | `sector2` | `erase_count2` | `CRC32`

Triplets are formed from non-zero erase counts and sectors (these two forming a pair). If not aligned by 3, remaining sector(s) and erase count(s) are filled with 0 => zero erase count marks an invalid record (why keep such record, when zero is the default).

So we have non-zero erase count per sector information in flash. This is what initializes `erase_count_buffer`. Such that the mentioned aggregation increments current total values for each sector.

Summary of algorithm with changes:

1. in `config()` calculate sectors needed for storing sector erase_count info in flash and "allocate" by subtracting from `flash_size`
2. in `init()` malloc a buffer big enough for `uint16_t` for each wear-levelled sector's erase count
3. also in `init()` try to read sector erase counts already stored in flash, initializing values in buffer
4. `updateWL(sector)` now receives a sector argument and writes new version of `pos update records` described above
5. once `pos` reaches `max_pos`, all `pos update records` are traversed and `buffer[sector]++` is performed
6. then triplets of pairs `(sector, erase count)` are formed and written to flash for future init of `erase_count_buffer`
7. `pos update records` are erased

TODO: second copy of `erase_count_records` to be reliable in keeping stats on erase counts.