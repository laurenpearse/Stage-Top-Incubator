%% Incubator chamber: Bode and Nichols analysis
% Frequency-domain analysis of the PI temperature controller.
% Needs the Control System Toolbox (check with: ver control)
clear; close all; clc;

%% Parameters
K      = 128;     % plant gain, K per unit duty
tau    = 594;     % plant time constant, s
lambda = 300;     % desired closed-loop time constant, s
b      = 0.5;     % setpoint weight

Kp = tau/(K*lambda);   % 0.0155
Ki = Kp/tau;           % 2.61e-5

s = tf('s');

%% Transfer functions
G    = K/(tau*s + 1);          % plant: duty -> chamber temperature
Gd_a = 0.701/(tau*s + 1);      % disturbance: ambient -> chamber
Gd_g = 0.299/(tau*s + 1);      % disturbance: glass -> chamber

C = Kp + Ki/s;                 % PI controller
L = minreal(C*G);              % loop transfer function (should be 1/(300s))

T  = feedback(L, 1);           % setpoint -> Tc, normal PI (b = 1)
F  = b*Kp + Ki/s;              % setpoint path with weighting
Tb = minreal(F*feedback(G, C));% setpoint -> Tc, with b = 0.5
S  = feedback(1, L);           % sensitivity (error / setpoint)
Ta_cl = minreal(Gd_a*S);       % ambient -> chamber, closed loop

% Frequency range: well below and above crossover (1/300 = 3.3e-3 rad/s)
w = logspace(-5, -1, 500);

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
figure('Name', 'Open-loop Bode');
bode(G, C, L, w);
grid on;
legend('Plant G(s)', 'Controller C(s)', 'Loop L(s) = C(s)G(s)', 'Location', 'southwest');
title('Open-loop Bode: plant, PI controller and loop');

%% Figure 2: Bode with margins marked
figure('Name', 'Margins');
margin(L);
grid on;

%% Figure 3: Closed-loop Bode
figure('Name', 'Closed-loop Bode');
bode(T, Tb, S, Ta_cl, w);
grid on;
legend('Setpoint to Tc (b = 1)', 'Setpoint to Tc (b = 0.5)', ...
       'Sensitivity S(s)', 'Ambient to Tc', 'Location', 'southwest');
title('Closed-loop frequency responses');

%% Figure 4: Nichols chart of the loop
figure('Name', 'Nichols');
nichols(L, w);
ngrid;
title('Nichols chart of L(s)');

%% Figure 5: Nichols with delay (sensor lag, sampling)
% A pure delay adds phase lag that grows with frequency, bending the
% curve to the left. Shows how much margin is left for real-world lag.
delays = [0 1.5 30 100];                 % seconds
figure('Name', 'Nichols with delay');
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
title('Nichols chart of L(s) with added delay');
hold off;

%% Figure 6: Step responses (sanity check against Simulink)
% 15 degC setpoint step (22 -> 37), linear model, no saturation.
figure('Name', 'Step responses');
t = 0:1:4000;
step(15*T, 15*Tb, t);
grid on;
legend('b = 1', 'b = 0.5', 'Location', 'southeast');
title('Linear setpoint step, 15 \circC (rise above ambient)');
ylabel('Temperature rise (\circC)');