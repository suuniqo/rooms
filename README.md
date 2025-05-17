
# rooms

rooms is a self hosted terminal messaging app.
rooms doesn't use any external libraries.
rooms uses end to end encryption (MLS).
rooms is fast, realiable and pretty.


## packet

 <- 1B -> <-- 8B --> <------ 16B ------> <- [1,230] B ->
+--------+----------+-------------------+---------------+
| length | msg time | usrname + padding | payload (msg) |
+--------+----------+-------------------+---------------+
 <-------------------- [26, 255] B -------------------->

this is the rooms header format:

 <---- 6B ----> <------- 12B -------->
+--------------+----------------------+
|     magic    |        usrname       |
+---+---+------+----------------------+
| l | f | chks |       options        |
+---+---+------+--+-------------------+
|      nonce      |     timestamp     |
+-----------------+-------------------+
|                                     :
:              payload                :
:                                     |
+-------------------------------------+
 <-------------- 16B ---------------->

 <- 1B -> <- 8B -> <-- 8B --> <------ 16B ------> <--- 1B ---> <- [1,221] B ->
+--------+--------+----------+-------------------+------------+---------------+
| length | msg id | msg time | usrname + padding | flag field | payload (msg) |
+--------+--------+----------+-------------------+------------+---------------+
 <------------------------------ [34, 255] B -------------------------------->

this is the flag field format:

  1b     1b    1b      1b     1b     1b     1b    1b
+-----+-----+------+-------+------+------+------+----+
| MSG | ACK | JOIN | LEAVE | WHSP | PING | PONG | NU |
+-----+-----+------+-------+------+------+------+----+
 <----------------------- 1B ----------------------->
 



next version?


## TODO

- scrolling in help section
- add join and left messages, no repeated users, and whisper feature?
- polish -> v0.1
- encryption
- [...]
- polish -> v1.0


## feature ideas

- add flag field
- add formating (bold and cursive)
- ee2e with MLS


## watch out

- replay attacks
- spoofing?
