## sqlite3-bloomfilter

Bloom-filter implementation for SQLite3.

Implements an aggregate, `bloomfilter(key[, size])` which returns a BLOB
corresponding to the internal state of the filter.

To do membership tests, use `bloom_contains(key, filter_state)`, passing the
BLOB from the `bloomfilter` aggregate as the 2nd parameter.

### Example python test script

```python

import sqlite3

# Load extension for use with in-memory database.
conn = sqlite3.connect(':memory:')
conn.enable_load_extension(True)
conn.load_extension('./bloom')

# Populate with some data.
conn.execute('create table register (data text not null)')
keys = ('foo', 'bar', 'baz')
for key in keys:
    conn.execute('insert into register (data) values (?)', (key,))

# Generate bloom filter for the above keys.
curs = conn.execute('select bloomfilter(data) from register')
filter_state = curs.fetchone()[0]

# Query the bloom filter:
for key in keys:
    curs = conn.execute('select bloom_contains(?, ?)', (key, filter_state))
    assert curs.fetchone()[0] == 1
```
