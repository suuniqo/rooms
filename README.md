
# rooms

rooms is a self hosted terminal messaging app.
rooms doesn't use any external libraries.
rooms uses end to end encryption (MLS).
rooms is fast, realiable and pretty.


## packet

initial packet (depreceated):

 <- 1B -> <-- 8B --> <------ 16B ------> <- [1,230] B ->
+--------+----------+-------------------+---------------+
| length | msg time | usrname + padding | payload (msg) |
+--------+----------+-------------------+---------------+
 <-------------------- [26, 255] B -------------------->

this is the rooms header format:

```
              <---- 6B ----> <------- 10B -------->
            +--------------+----------------------+
0x0000:     |     magic    |        usrname       |
            +---+---+------+----------------------+
0x0010:     | l | f | chks |       options        |
            +---+---+------+--+-------------------+
0x0020:     |      nonce      |     timestamp     |
            +-----------------+-------------------+
0x0030:     |                                     :
            :              payload                :
0x0130:     :                                     |
            +-------------------------------------+
             <-------------- 16B ---------------->
```

this is the flag field format:

  1b     1b    1b      1b     1b     1b     1b    1b
+-----+-----+------+-------+------+------+------+----+
| MSG | ACK | JOIN | LEAVE | WHSP | PING | PONG | NU |
+-----+-----+------+-------+------+------+------+----+
 <----------------------- 1B ----------------------->
 

## TODO

- polish palette and message gui format
- clean up gui especially for displaying the messages
- scrolling in help section
- no repeated users, and whisper feature?
- polish -> v0.1
- encryption
- [...]
- polish -> v1.0


## feature ideas

- add formating (bold and cursive)
- ee2e with MLS


## watch out

- replay attacks
- spoofing?
