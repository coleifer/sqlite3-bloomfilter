import sqlite3

conn = sqlite3.connect(':memory:')
conn.enable_load_extension(True)
conn.load_extension('./bloom')

conn.execute('create table register (id integer not null primary key, '
             'data text not null)')

s = ('foo', 'bar', 'baz', 'nuggie', 'huey', 'mickey', 'charlie', 'zaizee',
     'beanie', 'baze', 'ziggy')
for key in s:
    conn.execute('insert into register (data) values (?)', (key,))

curs = conn.execute('select bloomfilter(data) from register')
res, = curs.fetchall()[0]

for key in s:
    print conn.execute('select bloom_contains(?, ?)', (key, res)).fetchone()
print '----'
for key in s:
    key = key + '-test'
    print conn.execute('select bloom_contains(?, ?)', (key, res)).fetchone()
