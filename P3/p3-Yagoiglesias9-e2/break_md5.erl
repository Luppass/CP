%%ej 2
%%Carolina Grille Sieira
%%Santiago Iglesias Portela
-module(break_md5).
-define(PASS_LEN, 6).
-define(UPDATE_BAR_GAP, 1000).
-define(BAR_SIZE, 40).

-export([break_md5/1,
         pass_to_num/1,
         num_to_pass/1,
         num_to_hex_string/1,
         hex_string_to_num/1
        ]).

-export([progress_loop/3,
	 break_md5s/1]).

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
            io:format("\r[~s~s] ~.2f% ~16.w comprobaciones/segundo", [Full, Empty, N2/Bound*100, erlang:round(Checked/((Time_now-Time)/1000000))]), %%para sacar en seg divido entre 10⁶   io:format, el 16 deja 16 huecos para poner la cantidad
            progress_loop(N2, Bound, Time_now)
    end.

%% break_md5/2 iterates checking the possible passwords

break_md5(Target_Hash, N, N, _) -> {not_found, Target_Hash};  % Checked every possible password
break_md5(Target_Hash, N, Bound, Progress_Pid) ->
    if N rem ?UPDATE_BAR_GAP == 0 ->
            Progress_Pid ! {progress_report, ?UPDATE_BAR_GAP};
       true ->
            ok
    end,
    Pass = num_to_pass(N),
    Hash = crypto:hash(md5, Pass),
    Num_Hash = binary:decode_unsigned(Hash),
    if
        Target_Hash == Num_Hash ->
            io:format("\e[2K\r~.16B: ~s~n", [Num_Hash, Pass]);
        true ->
            break_md5(Target_Hash, N+1, Bound, Progress_Pid)
    end.

%% Break a hash

break_md5(Hash) ->
    Bound = pow(26, ?PASS_LEN),
    Progress_Pid = spawn(?MODULE, progress_loop, [0, Bound, erlang:monotonic_time(microsecond)]),  %%??????
    Num_Hash = hex_string_to_num(Hash),
    Res = break_md5(Num_Hash, 0, Bound, Progress_Pid),
    Progress_Pid ! stop,
    Res.
    
    
%% break_md5s/2 iterates checking the possible passwords searching for multiple hashes
break_md5s(Target_Hashes, N, N, _) -> {not_found, Target_Hashes};  % Checked every possible password
break_md5s(Target_Hashes, N, Bound, Progress_Pid) ->
    if N rem ?UPDATE_BAR_GAP == 0 ->
            Progress_Pid ! {progress_report, ?UPDATE_BAR_GAP};
       true ->
            ok
    end,
    Pass = num_to_pass(N),
    Hash = crypto:hash(md5, Pass),
    Num_Hash = binary:decode_unsigned(Hash),   
	Found =lists:member(Num_Hash,Target_Hashes),  %%la funcion member, si el elemento esta en la lista devuelve true, sino false
	Target_Hashes_Left = if Found ->
						io:format("\e[2K\r~.16B: ~s~n", [Num_Hash, Pass]),
						lists:delete(Num_Hash,Target_Hashes); %%la funcion delete, si en la lista esta el elemento devuelve la lista sin ese elemento
					true -> Target_Hashes
	end,
	if
		length(Target_Hashes_Left) > 0 -> %%length te dice cuantos elem tiene la lista
			break_md5s(Target_Hashes_Left, N+1, Bound, Progress_Pid);
		true ->
			ok
	end.


%% break list of hasheS
break_md5s(Hashes) ->
    Bound = pow(26, ?PASS_LEN),
    Progress_Pid = spawn(?MODULE, progress_loop, [0, Bound, erlang:monotonic_time(microsecond)]),
    Num_Hashes = [hex_string_to_num(Hash) || Hash <- Hashes], %%de lista hashes saco de uno en uno y le aplico hex_string_to_num
    Res = break_md5s(Num_Hashes, 0, Bound, Progress_Pid),
    Progress_Pid ! stop,
    Res.