clear all;
close all;

syms q
syms qGuard
syms qGuardMinExt
syms qGuardMinCOG
syms qGuardMinInt
syms qGuardMaxInt
syms qGuardMaxCOG
syms qGuardMaxExt
syms W_gamma
syms W_min

% This is for the beginning of the joints limits (the first window)
g_b=0.5*W_gamma*(1.0+tanh(-6.0*(q-qGuardMinCOG)/qGuard))+W_min;

% This is for the end of the joints limits (the last window)
g_e=0.5*W_gamma*(1.0+tanh( 6.0*(q-qGuardMaxCOG)/qGuard))+W_min;

diff_g_b=diff(g_b)
diff_g_e=diff(g_e)

%% Test example
q_Guard=8.0;
q_GuardMinExt=-2.0;
q_GuardMinCOG= 2.0;
q_GuardMinInt= 6.0;
q_GuardMaxInt=54.0;
q_GuardMaxCOG=58.0;
q_GuardMaxExt=62.0;
W__gamma=1.0;
W__min=1.0;

q=[-9:0.1:69];
for(i=1:size(q,2))
    disp(i);
    f_q(i)=func(q(i),q_GuardMinExt,q_GuardMinCOG,q_GuardMinInt, ...
                     q_GuardMaxInt,q_GuardMaxCOG,q_GuardMaxExt, ...
                           q_Guard,       W__min,     W__gamma);
end

for(i=1:size(q,2))
    disp(i);
    f_dot_q(i)=func_dot(q(i),q_GuardMinExt,q_GuardMinCOG,q_GuardMinInt, ...
                             q_GuardMaxInt,q_GuardMaxCOG,q_GuardMaxExt, ...
                                   q_Guard,       W__min,     W__gamma);
end

close; figure; hold on;plot(q,f_q);plot(q,f_dot_q,'r');
legend('function','derivative');
