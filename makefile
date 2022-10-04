server: main.cpp ./threadpool/threadpool.h ./threadpool/threadpool.cpp ./http/http_conn.h ./http/http_conn.cpp ./lock/locker.h ./log/log.h ./log/log.cpp ./cgi/connection_pool.h ./cgi/connection_pool.cpp ./timer/lst_timer.h  ./timer/lst_timer.cpp
	g++ -o server ./main.cpp ./threadpool/threadpool.h ./threadpool/threadpool.cpp ./http/http_conn.h ./http/http_conn.cpp ./lock/locker.h ./log/log.h ./log/log.cpp ./cgi/connection_pool.h ./cgi/connection_pool.cpp ./timer/lst_timer.h  ./timer/lst_timer.cpp -lpthread -lmysqlclient

clean:
	rm  -r server
