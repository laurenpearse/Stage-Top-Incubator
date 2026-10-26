%% Incubator glass: Bode and Nichols analysis
% Frequency-domain analysis of the PI glass temperature controller.
% Needs the Control System Toolbox (check with: ver control)
clear; close all; clc;

%% Parameters
K      = 73;      % glass plant gain, K per unit duty (16 W nominal ITO)
tau    = 158.3;   % glass time constant, s
lambda = 60;      % desired closed-loop time constant, s
b      = 0.5;     % setpoint weight

Kp = tau/(K*lambda);   % 0.0361
Ki = Kp/tau;           % 2.28e-4

s = tf('s');

%% Transfer functions
G    = K/(tau*s + 1);          % plant: ITO duty -> glass temperature
Gd_c = 0.383/(tau*s + 1);      % disturbance: chamber -> glass
Gd_a = 0.616/(tau*s + 1);      % disturbance: ambient -> glass

C = Kp + Ki/s;                 % PI controller
L = minreal(C*G);              % loop transfer function (should be 1/(60s))

T  = feedback(L, 1);           % setpoint -> Tg, normal PI (b = 1)
F  = b*Kp + Ki/s;              % setpoint path with weighting
Tb = minreal(F*feedback(G, C));% setpoint -> Tg, with b = 0.5
S  = feedback(1, L);           % sensitivity (error / setpoint)
Tc_cl = minreal(Gd_c*S);       % chamber -> glass, closed loop
Ta_cl = minreal(Gd_a*S);       % ambient -> glass, closed loop

% Frequency range: well below and above crossover (1/60 = 1.7e-2 rad/s)
w = logspace(-4, 0, 500);

%% Print the key numbers
disp('Controller C(s):'); C
disp('Loop L(s) = C(s)G(s), after cancellation:'); L

[Gm, Pm, Wcg, Wcp] = margin(L);
fprintf('\nGain margin:        %s\n', num2str(20*log10(Gm)));
fprintf('Phase margin:       %.1f deg\n', Pm);
fprintf('Crossover freq:     %.2e rad/s (1/lambda = %.2e)\n', Wcp, 1/lambda);
fprintf('Closed-loop BW:     %.2e rad/s\n', bandwidth(T));
fprintf('Closed-loop poles:  %s\n', mat2str(pole(Tb).', 4));

%% Figure 1: Bode of plant, controller and loop
figure('Name', 'Glass open-loop Bode');
bode(G, C, L, w);
grid on;
legend('Plant G_g(s)', 'Controller C_g(s)', 'Loop L(s) = C_g(s)G_g(s)', 'Location', 'southwest');
title('Glass open-loop Bode: plant, PI controller and loop');

%% Figure 2: Bode with margins marked
figure('Name', 'Glass margins');
margin(L);
grid on;

%% Figure 3: Closed-loop Bode
figure('Name', 'Glass closed-loop Bode');
bode(T, Tb, S, Tc_cl, Ta_cl, w);
grid on;
legend('Setpoint to Tg (b = 1)', 'Setpoint to Tg (b = 0.5)', ...
       'Sensitivity S(s)', 'Chamber to Tg', 'Ambient to Tg', 'Location', 'southwest');
title('Glass closed-loop frequency responses');

%% Figure 4: Nichols chart of the loop
figure('Name', 'Glass Nichols');
nichols(L, w);
ngrid;
title('Nichols chart of glass loop L(s)');

%% Figure 5: Nichols with delay (sensor lag, sampling)
% The glass loop is 5x faster than the chamber loop, so the same delay
% costs 5x more phase. Delays chosen to match likely sensor mountings.
delays = [0 1.5 10 30];                  % seconds
figure('Name', 'Glass Nichols with delay');
hold on;
names = strings(size(delays));
fprintf('\nEffect of delay:\n');
for k = 1:numel(delays)
    Ld = L*exp(-delays(k)*s);
    nichols(Ld, w);
    [~, Pm_d] = margin(Ld);
    fprintf('  delay %5.1f s  ->  phase margin %.1f deg\n', delays(k), Pm_d);
    names(k) = sprintf('delay = %g s', delays(k));
end
ngrid;
legend(names, 'Location', 'southwest');
title('Nichols chart of glass loop L(s) with added delay');
hold off;

%% Figure 6: Step responses (sanity check against Simulink)
% 17 degC setpoint step (22 -> 39), linear model, no saturation,
% chamber held constant (no coupling).
figure('Name', 'Glass step responses');
t = 0:1:1200;
step(17*T, 17*Tb, t);
grid on;
legend('b = 1', 'b = 0.5', 'Location', 'southeast');
title('Linear glass setpoint step, 17 \circC (rise above ambient)');
ylabel('Temperature rise (\circC)');

%% Figure 7: Discrete-time check (Ts = 1 s, as coded on the microcontroller)
Ts  = 1;
Gz  = c2d(G, Ts, 'zoh');               % plant with zero-order hold (PWM)
z   = tf('z', Ts);
Cz  = Kp + Ki*Ts/(z - 1);              % controller exactly as coded
Tz  = feedback(Cz*Gz, 1);
fprintf('\nDiscrete closed-loop poles: %s\n', mat2str(pole(Tz).', 6));
fprintf('Magnitudes (all < 1 = stable): %s\n', mat2str(abs(pole(Tz)).', 6));
fprintf('Continuous poles mapped to z:  %s\n', mat2str(exp(-Ts./[lambda tau]), 6));

figure('Name', 'Glass discrete vs continuous');
step(17*Tz, 17*T, t);
grid on;
legend('Discrete (Ts = 1 s)', 'Continuous', 'Location', 'southeast');
title('Glass loop: discrete vs continuous step, b = 1');
ylabel('Temperature rise (\circC)');