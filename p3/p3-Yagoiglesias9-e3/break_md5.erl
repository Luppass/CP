%%ej 3
%%Carolina Grille Sieira
%%Santiago Iglesias Portela
-module(break_md5).
-define(PASS_LEN, 6).
-define(UPDATE_BAR_GAP, 1000).
-define(BAR_SIZE, 40).
-define(NUM_PROCESSES, 7).

-export([pass_to_num/1,
         num_to_pass/1,
         num_to_hex_string/1,
         hex_string_to_num/1
        ]).

-export([progress_loop/3,
	 distribute_loop/2,
	 break_md5s/1,
	 break_md5s/4]).

% Base ^ Exp

pow_aux(_Base, Pow, 0) ->
    Pow;
pow_aux(Base, Pow, Exp) when Exp rem 2 == 0 ->
    pow_aux(Base*Base, Pow, Exp div 2);
pow_aux(Base, Pow, Exp) ->
    pow_aux(Base, Base * Pow, Exp - 1).

pow(Base, Exp) -> pow_aux(Base, 1, Exp).

%% Number to password and back conversion

num_to_pass_aux(_N, 0, Pass) -> Pass;
num_to_pass_aux(N, Digit, Pass) ->
    num_to_pass_aux(N div 26, Digit - 1, [$a + N rem 26 | Pass]).

num_to_pass(N) -> num_to_pass_aux(N, ?PASS_LEN, []).

pass_to_num(Pass) ->
    lists:foldl(fun (C, Num) -> Num * 26 + C - $a end, 0, Pass).

%% Hex string to Number

hex_char_to_int(N) ->
    if (N >= $0) and (N =< $9) -> N - $0;
       (N >= $a) and (N =< $f) -> N - $a + 10;
       (N >= $A) and (N =< $F) -> N - $A + 10;
       true                    -> throw({not_hex, [N]})
    end.

int_to_hex_char(N) ->
    if (N >= 0)  and (N < 10) -> $0 + N;
       (N >= 10) and (N < 16) -> $A + (N - 10);
       true                   -> throw({out_of_range, N})
    end.

hex_string_to_num(Hex_Str) ->
    lists:foldl(fun(Hex, Num) -> Num*16 + hex_char_to_int(Hex) end, 0, Hex_Str).

num_to_hex_string_aux(0, Str) -> Str;
num_to_hex_string_aux(N, Str) ->
    num_to_hex_string_aux(N div 16,
                          [int_to_hex_char(N rem 16) | Str]).

num_to_hex_string(0) -> "0";
num_to_hex_string(N) -> num_to_hex_string_aux(N, []).

%% Progress bar runs in its own process

progress_loop(N, Bound, Time) ->
    receive
        stop -> ok;
        {progress_report, Checked} ->
	    Time_now = erlang:monotonic_time(microsecond),
            N2 = N + Checked,
            Full_N = N2 * ?BAR_SIZE div Bound,
            Full = lists:duplicate(Full_N, $=),
            Empty = lists:duplicate(?BAR_SIZE - Full_N, $-),
            io:format("\r[~s~s] ~.2f% ~16.w comprobaciones/segundo ", [Full, Empty, N2/Bound*100, erlang:round(Checked/((Time_now-Time)/1000000))]), %%para sacar en seg divido entre 10⁶   io:format, el 16 deja 16 huecos para poner la cantidad
            progress_loop(N2, Bound, Time_now)
    end.

 
%%%%%%%
%% everyone has their own process
distribute_loop(Cont,Bound) ->
    receive
        stop -> 
			io:format("Fin distributer~n"),
			ok;
        {final_block, Breaker} ->
        	%io:format("Asignado bloque ~w a PID:~w~n",[Cont,Breaker]),
			Breaker ! {new_block, Cont}
	end,		
    distribute_loop(Cont + ?UPDATE_BAR_GAP, Bound).
    
%% break_md5s/2 iterates checking the possible passwords searching for multiple hashes

break_md5s(Target_Hashes, Progress_PID, Distribute_PID, Notify_PID) ->
	Bound = pow(26, ?PASS_LEN),
	Distribute_PID ! {final_block,self()},
	receive
		{new_block, Pass_Cont} -> 
		%io:format("Proceso ~w arrancando con pass_num:~w~n",[self(),Pass_Cont]),
		break_md5s(Target_Hashes, Progress_PID, Distribute_PID, Notify_PID, Pass_Cont, Bound)
	end.
break_md5s(Target_Hashes, _, _, _, Bound, Bound) -> 
	{not_found, Target_Hashes};  % Checked every possible password
break_md5s(Target_Hashes, Progress_PID, Distribute_PID, Notify_PID, N, Bound) ->
	receive
		{borrar, deleteHash} -> 
			Updated_list = lists:delete(deleteHash,Target_Hashes),
			%io:format("El Breaker ~w borra 1 hash de la lista~n",[self()]),
			break_md5s(Updated_list, Progress_PID, Distribute_PID, Notify_PID, N, Bound)
		after 0 ->
			true
	end,
	Pass = num_to_pass(N),
	Hash = crypto:hash(md5, Pass),
	Num_Hash = binary:decode_unsigned(Hash),   
	Foundd =lists:member(Num_Hash,Target_Hashes),
	Left_Target_Hashes = if Foundd ->
									io:format("\e[2K\r~.16B: ~s ~w~n", [Num_Hash, Pass, self()]),
									Notify_PID ! {found, Num_Hash, self()},
									lists:delete(Num_Hash,Target_Hashes);
								true -> Target_Hashes
	end,
	if
		length(Left_Target_Hashes) > 0 ->
			if 	N rem (?UPDATE_BAR_GAP-1) == 0 ->
					Progress_PID ! {progress_report, ?UPDATE_BAR_GAP},
					Distribute_PID ! {final_block,self()},
					receive
						{new_block, Pass_Cont} -> 
							%io:format("Proceso Breaker ~w recibe el bloque ~w~n",[self(),Pass_Cont]),
							break_md5s(Left_Target_Hashes, Progress_PID, Distribute_PID, Notify_PID, Pass_Cont, Bound)
					end;
				true ->
				
break_md5s(Left_Target_Hashes, Progress_PID, Distribute_PID, Notify_PID, N+1, Bound)			
			end;
		true ->
			io:format("Fin del proceso Breaker ~w~n",[self()])
	end.


%% break list of hasheS
break_md5s(Hashes) ->
    Bound = pow(26, ?PASS_LEN),
    Progress_PID = spawn(?MODULE, progress_loop, [0, Bound, erlang:monotonic_time(microsecond)]),
    Distribute_PID = spawn(?MODULE, distribute_loop, [0, Bound]),
    Num_Hashes = [hex_string_to_num(Hash) || Hash <- Hashes],
    Breakers_Pids = starts_breakers([], Num_Hashes, ?NUM_PROCESSES, 0, Progress_PID, Distribute_PID),
    io:format("~w~n",[Breakers_Pids]),
	Res = notify_loop(Breakers_Pids,Num_Hashes),
    Progress_PID ! stop,
    Distribute_PID ! stop,
    io:format("Finalizando proceso ~w Principal ~n",[self()]),
    Res.

%% notification of found passwords
notify_loop(_,[]) ->
	io:format("Fin notify~n"),
	ok;
notify_loop(Breakers,Num_Hashes) ->
	receive
		{found, Hash, PorPid} ->
			[B ! {borrar, Hash} || B <- Breakers, B/=PorPid],
			notify_loop(Breakers,lists:delete(Hash,Num_Hashes))
	end.

%% starts breakers
starts_breakers(Pids, _, NumProcs, NumProcs, _, _) ->
	Pids;
starts_breakers(Pids, Num_Hashes, NumProcs, N, Progress_PID, Distribute_PID) ->
	Pid = spawn(?MODULE,break_md5s,[Num_Hashes, Progress_PID, Distribute_PID, self()]),
	starts_breakers(Pids ++ [Pid],Num_Hashes, NumProcs, N+1, Progress_PID, Distribute_PID).
