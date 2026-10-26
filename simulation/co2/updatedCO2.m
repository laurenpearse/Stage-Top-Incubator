%% Incubator CO2 control: ALL report figures  [FINAL, built chamber]
% ========================================================================
%  This is the ONLY file you need. Delete CO2figures_UPDATED.m and
%  CO2_fig_C8_only.m / _v2.m.
%
%  Updated for the built chamber:
%    V     0.99   ->  0.47  L
%    QL    5.5e-4 ->  2.611e-4 L/s   (still 2 air changes per hour)
%    K     273.6  ->  576.3 pp per unit duty
%    tau   1711   ->  1711  s        (set by the exchange rate, not V)
%    Kp    0.0313 ->  0.0148
%    Ki    1.83e-5 -> 8.68e-6
%
%  Figures produced:  C2 fill, C3 fill strategies, C4 lid openings,
%                     C5 robustness, C6 valve pulses, C7 Bode, C8 sweeps.
%  The Simulink diagram figure (old C1) is dropped.
%
%  Everything is saved to a SEPARATE folder with every file prefixed NEW_,
%  so the originals are untouched.
% ========================================================================

clear; close all; clc;

%% ======================= SETTINGS =======================
figW   = 16;             % figure width in cm
dpi    = 600;            % PNG resolution
outDir = fullfile(pwd, 'figures', 'updated');
pre    = 'NEW_';         % every saved file starts with this

set(groot, 'defaultTextInterpreter', 'latex', ...
           'defaultAxesTickLabelInterpreter', 'latex', ...
           'defaultLegendInterpreter', 'latex', ...
           'defaultAxesFontSize', 10, ...
           'defaultLineLineJoin', 'round');

%% ======================= PARAMETERS =======================
% ---- Plant ----
P.V     = 0.47;          % free gas volume, L
P.Qin   = 0.1/60;        % flow with valve open, L/s (0.1 L/min)
P.QL    = 2.611e-4;      % leak exchange flow, L/s = 2 air changes per hour
P.xs    = 100;           % supply CO2, %
P.xa    = 0.04;          % room CO2, %
P.tsen  = 13;            % ExplorIR-M time constant, s (T90 < 30 s)
% ---- Controller and actuator ----
P.xsp   = 5;             % setpoint, %
P.lam   = 200;           % lambda, s
P.Ts    = 15;            % control period, s
P.tmin  = 0.1;           % minimum valve pulse, s
P.tread = 0.5;           % sensor read interval, s
P.avg   = 5;             % averaging window for readings, s
P.band  = 0.5;           % integrator freeze band, percentage points
P.umax  = 1;             % duty limit
P.dt    = 0.02;          % integration step, s (resolves 0.1 s pulses)
% ---- Design model (controller tuning; never changed during a sweep) ----
D.K = 576.3; D.tau = 1711;

Kp = D.tau/(D.K*P.lam);  Ki = 1/(D.K*P.lam);

% ---- Banner so the right parameter set is obvious -----------------------
mu0_design = P.QL*(P.xsp - P.xa)/(P.Qin*(P.xs - P.xsp));
K_check    = P.Qin*(P.xs - P.xsp)/(mu0_design*P.Qin + P.QL);
tau_check  = P.V/(mu0_design*P.Qin + P.QL);
fprintf('============================================================\n');
fprintf(' CO2 PARAMETERS, BUILT CHAMBER\n');
fprintf('------------------------------------------------------------\n');
fprintf('  V      = %.3f L          (was 0.99)\n', P.V);
fprintf('  QL     = %.4e L/s = %.2f L/h = %.1f air changes/h\n', ...
        P.QL, P.QL*3600, P.QL*3600/P.V);
fprintf('  mu0    = %.3f %%           (was 1.72)\n', 100*mu0_design);
fprintf('  K      = %.1f pp         (design model %.1f)\n', K_check, D.K);
fprintf('  tau    = %.0f s           (design model %.0f)\n', tau_check, D.tau);
fprintf('  Kp     = %.4f per pp     (was 0.0313)\n', Kp);
fprintf('  Ki     = %.3e per pp.s (was 1.83e-05)\n', Ki);
fprintf('  one minimum pulse adds %.4f pp\n', P.Qin*P.tmin*(P.xs - P.xsp)/P.V);
fprintf('  finest duty the valve can make %.2f %%, steady duty %.2f %%\n', ...
        100*P.tmin/P.Ts, 100*mu0_design);
fprintf('  output: %s\n', outDir);
fprintf('============================================================\n\n');

% ---- Default run options ----
O.mode    = 'pi';        % 'pi' or 'fullopen'
O.fillTo  = 4;           % full-open fill: switch to PI at this reading, %
O.startSS = false;       % true: start settled at 5%
O.door    = [];          % time of a sudden CO2 loss (lid opening), s
O.doorTo  = 1;           % CO2 right after the loss, %
O.freeze  = false;       % integrator freeze during large errors
O.QLx     = 1;           % leak multiplier (plant only)
O.Qinx    = 1;           % flow multiplier (plant only)
O.Tend    = 60*60;       % run length, s

% ---- Colours ----
C.co2  = [0.494 0.184 0.556];
C.sen  = [0.15 0.15 0.15];
C.sp   = [0.45 0.45 0.45];
C.bad  = [0.635 0.078 0.184];
C.alt  = [0.850 0.325 0.098];
C.duty = [0.466 0.674 0.188];
C.seq  = [0.78 0.62 0.84; 0.494 0.184 0.556; 0.25 0.05 0.30];
C.seqg = [0.70 0.85 0.50; 0.466 0.674 0.188; 0.20 0.40 0.05];

pct = '\%';
if ~exist(outDir, 'dir'), mkdir(outDir); end

%% ======================= FIG C2: FILL FROM ROOM AIR =======================
R = simCO2(P, D, O);
M = metrics(R, 0, P.xsp);
fprintf('Fig C2 fill from room air:\n');
fprintf('  overshoot %.2f pp, 63.2%% at %.1f min, starting duty %.3f, final duty %.4f\n', ...
        M.overshoot, time632(R, P.xsp), R.u(1), mean(R.u(R.tmin > 40)));
fprintf('  ripple in the last 20 min: %.3f pp\n\n', M.ripple);

fig = newFig(figW, 12, 'C2 Fill');
tl = tiledlayout(fig, 3, 3, 'TileSpacing', 'compact', 'Padding', 'compact');
ax1 = nexttile(tl, 1, [2 2]); hold(ax1, 'on');
h0 = plot(ax1, R.tmin, R.x, 'Color', C.seq(1, :), 'LineWidth', 0.6);
h1 = plot(ax1, R.tmin, R.xm, 'Color', C.co2, 'LineWidth', 1.8);
h2 = plot(ax1, R.tmin, R.y, '--', 'Color', C.sen, 'LineWidth', 1.1);
yline(ax1, P.xsp, ':', 'Color', C.sp, 'LineWidth', 1.0);
ylabel(ax1, ['CO$_2$ (' pct ')']); ylim(ax1, [0 6]); xlim(ax1, [0 20]);
mkLegend(ax1, {'Chamber CO$_2$, 15 s average', 'Instantaneous (valve pulses)', ...
               'Sensor reading (ExplorIR-M)'}, 'southeast', [h1 h0 h2]);
styleAxes(ax1); set(ax1, 'XTickLabel', []);
panelTitle(ax1, '(a)', 'Chamber CO$_2$ during the fill');

axZ = nexttile(tl, 3, [2 1]); hold(axZ, 'on');
fill(axZ, [0 20 20 0], P.xsp + [-0.2 -0.2 0.2 0.2], C.sp, 'FaceAlpha', 0.12, 'EdgeColor', 'none');
plot(axZ, R.tmin, R.x, 'Color', C.seq(1, :), 'LineWidth', 0.6);
plot(axZ, R.tmin, R.xm, 'Color', C.co2, 'LineWidth', 1.8);
plot(axZ, R.tmin, R.y, '--', 'Color', C.sen, 'LineWidth', 1.1);
yline(axZ, P.xsp, ':', 'Color', C.sp);
xlim(axZ, [5 20]); ylim(axZ, [4.6 5.2]);
styleAxes(axZ); set(axZ, 'XTickLabel', []);
panelTitle(axZ, '(b)', 'Close to 5\%');
text(axZ, 0.95, 0.06, '$\pm$0.2 pp band', 'Units', 'normalized', ...
     'HorizontalAlignment', 'right', 'FontSize', 8.5, 'Color', C.sp);

ax2 = nexttile(tl, 7, [1 2]); hold(ax2, 'on');
stairs(ax2, R.tmin, 100*R.u, 'Color', C.duty, 'LineWidth', 1.5);
ylabel(ax2, ['Duty (' pct ')']); xlabel(ax2, 'Time (min)');
ylim(ax2, [0, 5*ceil(100*max(R.u)/5)]); xlim(ax2, [0 20]);
styleAxes(ax2); panelTitle(ax2, '(c)', 'Solenoid duty, updated every 15 s');

ax3 = nexttile(tl, 9); hold(ax3, 'on');
stairs(ax3, R.tmin, 100*R.u, 'Color', C.duty, 'LineWidth', 1.5);
xlim(ax3, [5 20]); ylim(ax3, [0 5]); xlabel(ax3, 'Time (min)');
styleAxes(ax3); panelTitle(ax3, '(d)', 'Duty, close up');
saveFig(fig, [pre 'fig_C2_fill'], outDir, dpi);

%% ======================= FIG C3: PI FILL VS FULL-OPEN FILL =======================
Q = O; Q.mode = 'fullopen'; Rfo = simCO2(P, D, Q);
Mfo = metrics(Rfo, 0, P.xsp);
fprintf('Fig C3: PI fill        peak %.2f %%\n', max(R.xm));
fprintf('        full-open fill peak %.2f %%  (would trip the 3 %% test abort)\n\n', max(Rfo.xm));

fig = newFig(figW, 11, 'C3 Fill strategies');
tl = tiledlayout(fig, 1, 3, 'TileSpacing', 'compact', 'Padding', 'compact');
ax = nexttile(tl, [1 2]); hold(ax, 'on');
h1 = plot(ax, Rfo.tmin, Rfo.xm, 'Color', C.bad, 'LineWidth', 1.8);
h2 = plot(ax, R.tmin, R.xm, 'Color', C.co2, 'LineWidth', 1.8);
yline(ax, P.xsp, ':', 'Color', C.sp, 'LineWidth', 1.0);
yline(ax, O.fillTo, '--', 'Color', C.bad, 'LineWidth', 0.8);
text(ax, 58, O.fillTo - 0.25, 'Valve closes when sensor reads 4\%', ...
     'HorizontalAlignment', 'right', 'Color', C.bad, 'FontSize', 8.5);
ylabel(ax, ['Chamber CO$_2$, 15 s average (' pct ')']); xlabel(ax, 'Time (min)');
xlim(ax, [0 60]); ylim(ax, [0 ceil(max(Rfo.x)) + 0.5]);
mkLegend(ax, {sprintf('Valve fully open, then PI: peak %.2f\\%%', max(Rfo.xm)), ...
              sprintf('PI from the start: peak %.2f\\%%', max(R.xm))}, 'southeast', [h1 h2]);
styleAxes(ax); panelTitle(ax, '(a)', 'Filling from room air');

ax = nexttile(tl); hold(ax, 'on');
h1 = plot(ax, Rfo.t, Rfo.x, 'Color', C.bad, 'LineWidth', 1.8);
h2 = plot(ax, Rfo.t, Rfo.y, '--', 'Color', C.sen, 'LineWidth', 1.1);
yline(ax, P.xsp, ':', 'Color', C.sp);
xlim(ax, [0 90]); ylim(ax, [0 ceil(max(Rfo.x)) + 0.5]);
xlabel(ax, 'Time (s)');
mkLegend(ax, {'True CO$_2$', 'Sensor'}, 'southeast', [h1 h2]);
styleAxes(ax); panelTitle(ax, '(b)', 'First 90 s, full-open');
saveFig(fig, [pre 'fig_C3_fill_strategies'], outDir, dpi);

%% ======================= FIG C4: LID OPENINGS =======================
tDoor = 10*60;
drops = [4 1];
fig = newFig(figW, 13, 'C4 Lid openings');
tl = tiledlayout(fig, 2, 2, 'TileSpacing', 'compact', 'Padding', 'compact');
fprintf('Fig C4 lid openings (start settled at 5%%):\n');
for j = 1:2
    Q = O; Q.startSS = true; Q.door = tDoor; Q.doorTo = drops(j); Q.Tend = tDoor + 60*60;
    Q.freeze = false; Rp = simCO2(P, D, Q);
    Q.freeze = true;  Rf = simCO2(P, D, Q);
    Mp = metrics(Rp, tDoor, P.xsp); Mf = metrics(Rf, tDoor, P.xsp);
    fprintf('  drop to %g%%: plain PI peak +%.2f pp | with freeze peak +%.2f pp\n', ...
            drops(j), Mp.overshoot, Mf.overshoot);
    tt = (Rp.t - tDoor)/60;

    ax = nexttile(tl, j); hold(ax, 'on');
    h1 = plot(ax, tt, Rp.xm, ':', 'Color', C.alt, 'LineWidth', 1.8);
    h2 = plot(ax, tt, Rf.xm, '-', 'Color', C.co2, 'LineWidth', 1.8);
    yline(ax, P.xsp, ':', 'Color', C.sp);
    xlim(ax, [-2 60]); ylim(ax, [0 6]);
    ylabel(ax, ['CO$_2$, 15 s average (' pct ')']);
    mkLegend(ax, {'Plain PI', 'PI with integrator freeze'}, 'southeast', [h1 h2]);
    styleAxes(ax);
    panelTitle(ax, sprintf('(%s)', char('a' + j - 1)), ...
               sprintf('Lid opened, CO$_2$ drops to %g\\%%', drops(j)));

    ax = nexttile(tl, j + 2); hold(ax, 'on');
    fill(ax, [-2 60 60 -2], P.xsp + [-0.2 -0.2 0.2 0.2], C.sp, 'FaceAlpha', 0.12, 'EdgeColor', 'none');
    h1 = plot(ax, tt, Rp.xm, ':', 'Color', C.alt, 'LineWidth', 1.8);
    h2 = plot(ax, tt, Rf.xm, '-', 'Color', C.co2, 'LineWidth', 1.8);
    yline(ax, P.xsp, ':', 'Color', C.sp);
    xlim(ax, [-2 60]); ylim(ax, [4.4 5.6]);
    ylabel(ax, ['CO$_2$ (' pct ')']); xlabel(ax, 'Time since lid opened (min)');
    mkLegend(ax, {sprintf('Plain PI, peak +%.2f pp', Mp.overshoot), ...
                  sprintf('With freeze, peak +%.2f pp', Mf.overshoot)}, 'southeast', [h1 h2]);
    styleAxes(ax);
    panelTitle(ax, sprintf('(%s)', char('c' + j - 1)), 'Close to setpoint');
end
fprintf('\n');
saveFig(fig, [pre 'fig_C4_lid_openings'], outDir, dpi);

%% ======================= FIG C5: ROBUSTNESS =======================
% Leak cases run UP from the design value: the built chamber has an unsealed
% gland and an unclamped lid, so it is more likely leakier than 2 ACH.
QLm  = [1 3 6];        Qinm = [0.5 1 2];
QLlab  = {'$Q_L \times 1$ (design, 2/h)', '$Q_L \times 3$ (6/h)', '$Q_L \times 6$ (12/h)'};
Qinlab = {'$Q_{in} \times 0.5$ (0.05 L/min)', '$Q_{in} \times 1$ (design, 0.1 L/min)', ...
          '$Q_{in} \times 2$ (0.2 L/min)'};
Trob = 90*60;
tab = {};
fig = newFig(figW, 20, 'C5 Robustness');
tl = tiledlayout(fig, 6, 2, 'TileSpacing', 'compact', 'Padding', 'compact');
axA = nexttile(tl, 1, [2 1]); hold(axA, 'on');
axB = nexttile(tl, 2, [2 1]); hold(axB, 'on');
axC = nexttile(tl, 5, [2 1]); hold(axC, 'on');
axD = nexttile(tl, 6, [2 1]); hold(axD, 'on');
for ax = [axC axD]
    fill(ax, [0 90 90 0], P.xsp + [-0.2 -0.2 0.2 0.2], C.sp, 'FaceAlpha', 0.12, 'EdgeColor', 'none');
end
hA = gobjects(1, 3); hB = gobjects(1, 3);
for i = 1:3
    lw = 1.3 + 0.6*(QLm(i) == 1);
    Q = O; Q.QLx = QLm(i); Q.Tend = Trob; Ri = simCO2(P, D, Q); Mi = metrics(Ri, 0, P.xsp);
    hA(i) = plot(axA, Ri.tmin, Ri.xm, 'Color', C.seq(i, :), 'LineWidth', lw);
    plot(axC, Ri.tmin, Ri.xm, 'Color', C.seq(i, :), 'LineWidth', lw);
    tbi = sprintf('%.1f', time632(Ri, P.xsp));
    if QLm(i) == 1
        tab = [{'Design values', Mi.overshoot, tbi, Mi.ripple}; tab]; %#ok<AGROW>
    else
        tab(end+1, :) = {sprintf('Leak $\\times %g$', QLm(i)), Mi.overshoot, tbi, Mi.ripple}; %#ok<SAGROW>
    end

    lw = 1.3 + 0.6*(Qinm(i) == 1);
    Q = O; Q.Qinx = Qinm(i); Q.Tend = Trob; Ri = simCO2(P, D, Q); Mi = metrics(Ri, 0, P.xsp);
    hB(i) = plot(axB, Ri.tmin, Ri.xm, 'Color', C.seqg(i, :), 'LineWidth', lw);
    plot(axD, Ri.tmin, Ri.xm, 'Color', C.seqg(i, :), 'LineWidth', lw);
    if Qinm(i) ~= 1
        tab(end+1, :) = {sprintf('Flow $\\times %g$', Qinm(i)), Mi.overshoot, ...
                         sprintf('%.1f', time632(Ri, P.xsp)), Mi.ripple}; %#ok<SAGROW>
    end
end
for ax = [axA axB]
    yline(ax, P.xsp, ':', 'Color', C.sp); xlim(ax, [0 90]); ylim(ax, [0 6]);
    ylabel(ax, ['CO$_2$ (' pct ')']); styleAxes(ax); set(ax, 'XTickLabel', []);
end
for ax = [axC axD]
    yline(ax, P.xsp, ':', 'Color', C.sp); xlim(ax, [0 90]); ylim(ax, [4.4 5.4]);
    ylabel(ax, ['CO$_2$ (' pct ')']); xlabel(ax, 'Time (min)'); styleAxes(ax);
end
mkLegend(axA, QLlab, 'southeast', hA); mkLegend(axB, Qinlab, 'southeast', hB);
panelTitle(axA, '(a)', 'Leak rate varied');             panelTitle(axB, '(b)', 'Valve flow varied');
panelTitle(axC, '(c)', 'Leak rate, close to setpoint'); panelTitle(axD, '(d)', 'Valve flow, close to setpoint');

axT = nexttile(tl, 9, [2 2]);
drawTable(axT, {'Plant case (controller unchanged)', 'Overshoot (pp)', ...
                'Time to 63.2\% (min)', 'Ripple (pp)'}, tab);
saveFig(fig, [pre 'fig_C5_robustness'], outDir, dpi);

fprintf('Fig C5 robustness:\n');
fprintf('  %-16s %10s %14s %10s\n', 'Case', 'Overshoot', 'To 4.8%', 'Ripple');
for i = 1:size(tab, 1)
    fprintf('  %-16s %7.2f pp %10.1f min %7.3f pp\n', ...
            strrep(strrep(tab{i, 1}, '$\times ', 'x'), '$', ''), tab{i, 2}, tab{i, 3}, tab{i, 4});
end
fprintf('  LaTeX table rows:\n');
for i = 1:size(tab, 1)
    fprintf('  %s & %.2f & %s & %.3f \\\\\n', tab{i, 1}, tab{i, 2}, tab{i, 3}, tab{i, 4});
end
fprintf('\n');

%% ======================= FIG C6: VALVE PULSES AT STEADY STATE =======================
Q = O; Q.startSS = true; Q.Tend = 12*60;
Rss = simCO2(P, D, Q);
w0 = 8*60; w1 = 10*60;
win = Rss.t >= w0 & Rss.t <= w1;
tw  = Rss.t(win) - w0;
onT = sum(Rss.v(win))*P.dt/((w1 - w0)/P.Ts);
fprintf('Fig C6: average valve-open time per 15 s window %.2f s (minimum pulse %.2f s)\n', ...
        onT, P.tmin);
fprintf('        CO2 ripple %.3f pp\n\n', max(Rss.x(win)) - min(Rss.x(win)));

fig = newFig(figW, 10, 'C6 Valve pulses');
tl = tiledlayout(fig, 3, 1, 'TileSpacing', 'compact', 'Padding', 'compact');
ax1 = nexttile(tl, [2 1]); hold(ax1, 'on');
h1 = plot(ax1, tw, Rss.x(win), 'Color', C.co2, 'LineWidth', 1.6);
h2 = plot(ax1, tw, Rss.y(win), '--', 'Color', C.sen, 'LineWidth', 1.1);
yline(ax1, P.xsp, ':', 'Color', C.sp);
ylabel(ax1, ['CO$_2$ (' pct ')']);
yl = [min(Rss.x(win)) max(Rss.x(win))]; pad = 0.6*diff(yl) + 0.01;
ylim(ax1, [yl(1) - pad, yl(2) + pad]);
mkLegend(ax1, {'True chamber CO$_2$', 'Sensor reading'}, 'northeast', [h1 h2]);
styleAxes(ax1); panelTitle(ax1, '(a)', 'CO$_2$ ripple at steady state');

ax2 = nexttile(tl); hold(ax2, 'on');
area(ax2, tw, Rss.v(win), 'FaceColor', C.duty, 'EdgeColor', C.duty, 'FaceAlpha', 0.8);
ylim(ax2, [0 1.3]); yticks(ax2, [0 1]); yticklabels(ax2, {'Closed', 'Open'});
xlabel(ax2, 'Time (s)'); ylabel(ax2, 'Valve');
styleAxes(ax2);
panelTitle(ax2, '(b)', sprintf('Solenoid: one pulse per 15 s window, average %.2f s open', onT));
% NOTE: linkaxes removed on purpose, it hangs on some macOS MATLAB builds.
xlim(ax1, [0 w1 - w0]); xlim(ax2, [0 w1 - w0]); set(ax1, 'XTickLabel', []);
saveFig(fig, [pre 'fig_C6_valve_pulses'], outDir, dpi);

%% ======================= FIG C7: BODE WITH SENSOR LAG =======================
s  = tf('s');
G  = D.K/(D.tau*s + 1);
Cc = Kp + Ki/s;
L0 = minreal(Cc*G);
Lr = Cc*G/(P.tsen*s + 1)*exp(-(P.Ts/2 + P.avg/2)*s);
Lm = Lr/(30*s + 1);
w  = logspace(-5, -0.5, 800);
[m0, p0] = bode(L0, w); m0 = 20*log10(squeeze(m0)); p0 = squeeze(p0);
[mr, pr] = bode(Lr, w); mr = 20*log10(squeeze(mr)); pr = squeeze(pr);
[mm, pm] = bode(Lm, w); mm = 20*log10(squeeze(mm)); pm = squeeze(pm);
[~, Pm0] = margin(L0); [~, Pmr, ~, Wr] = margin(Lr); [~, Pmm] = margin(Lm);
fprintf('Fig C7 phase margins: ideal %.1f deg, sensor+sampling %.1f deg, plus mixing %.1f deg\n', ...
        Pm0, Pmr, Pmm);
fprintf('        crossover %.4f rad/s = 1/lambda\n\n', Wr);

fig = newFig(figW, 12, 'C7 Bode');
tl = tiledlayout(fig, 2, 1, 'TileSpacing', 'compact', 'Padding', 'compact');
ax1 = nexttile(tl); hold(ax1, 'on');
h1 = plot(ax1, w, m0, '--', 'Color', C.sp, 'LineWidth', 1.4);
h2 = plot(ax1, w, mr, '-', 'Color', C.co2, 'LineWidth', 1.8);
h3 = plot(ax1, w, mm, '-', 'Color', C.seq(1, :), 'LineWidth', 1.6);
yline(ax1, 0, '-', 'Color', C.sp);
xline(ax1, Wr, ':', 'Color', C.co2, 'LineWidth', 1.1);
set(ax1, 'XScale', 'log'); xticks(ax1, 10.^(-5:0)); xlim(ax1, [w(1) w(end)]);
ylabel(ax1, 'Magnitude (dB)');
mkLegend(ax1, {'Ideal loop, $1/(200s)$', 'With sensor lag, sampling and averaging', ...
               'Plus 30 s mixing lag'}, 'southwest', [h1 h2 h3]);
styleAxes(ax1); set(ax1, 'XTickLabel', []); panelTitle(ax1, '(a)', 'Loop gain');

ax2 = nexttile(tl); hold(ax2, 'on');
h1 = plot(ax2, w, p0, '--', 'Color', C.sp, 'LineWidth', 1.4);
h2 = plot(ax2, w, pr, '-', 'Color', C.co2, 'LineWidth', 1.8);
h3 = plot(ax2, w, pm, '-', 'Color', C.seq(1, :), 'LineWidth', 1.6);
yline(ax2, -180, '-', 'Color', C.bad, 'LineWidth', 1.1);
xline(ax2, Wr, ':', 'Color', C.co2, 'LineWidth', 1.1);
set(ax2, 'XScale', 'log'); xticks(ax2, 10.^(-5:0)); xlim(ax2, [w(1) w(end)]);
ylim(ax2, [-270 0]); yticks(ax2, -270:45:0);
xlabel(ax2, 'Frequency (rad/s)'); ylabel(ax2, 'Phase ($^\circ$)');
mkLegend(ax2, {sprintf('Ideal, PM = %.0f$^\\circ$', Pm0), ...
               sprintf('Realistic, PM = %.0f$^\\circ$', Pmr), ...
               sprintf('With mixing, PM = %.0f$^\\circ$', Pmm)}, 'southwest', [h1 h2 h3]);
text(ax2, 0.98, 0.30, 'Stability limit, $-180^\circ$', 'Units', 'normalized', ...
     'HorizontalAlignment', 'right', 'Color', C.bad, 'FontSize', 9);
styleAxes(ax2); panelTitle(ax2, '(b)', 'Loop phase (dotted line: crossover frequency)');
saveFig(fig, [pre 'fig_C7_bode'], outDir, dpi);

%% ======================= FIG C8: LEAK AND FLOW SWEEPS =======================
% K/tau = Qin*(xs - x0)/V, so the LEAK cancels: a leakier chamber has a
% smaller gain and a shorter time constant in the same proportion, crossover
% and phase margin are unchanged. What suffers is the fill, because lambda
% tuning sets Ti = tau and the controller keeps using 1711 s.
% mu0*Qin does not depend on Qin, so tau is independent of the FLOW and the
% whole of a flow error lands on K: crossover moves to m/lambda.
fprintf('------------------------------------------------------------\n');
fprintf('Fig C8: leak and flow sweeps\n');

ACH  = 1:14;
tbL  = nan(size(ACH)); rpL = tbL; duL = tbL; tauL = tbL; KL = tbL;
fprintf('  leak ');
for i = 1:numel(ACH)
    Q = O; Q.QLx = ACH(i)/2; Q.Tend = 180*60;
    Ri = simCO2(P, D, Q);
    rpL(i) = rippleOf(Ri);
    duL(i) = 100*mean(Ri.u(Ri.tmin > 120));
    tbL(i) = time632(Ri, P.xsp);
    QLi  = P.QL*Q.QLx;
    mu0i = QLi*(P.xsp - P.xa)/(P.Qin*(P.xs - P.xsp));
    Di   = mu0i*P.Qin + QLi;
    KL(i)   = P.Qin*(P.xs - P.xsp)/Di;
    tauL(i) = P.V/Di;
    fprintf('.');
end
fprintf(' done\n  flow ');

Qr   = [0.5 0.75 1 1.5 2 3];
tbF  = nan(size(Qr)); rpF = tbF; pmF = tbF; wcF = tbF;
theta_mix = 60;
for i = 1:numel(Qr)
    Q = O; Q.Qinx = Qr(i); Q.Tend = 180*60;
    Ri = simCO2(P, D, Q);
    rpF(i) = rippleOf(Ri);
    tbF(i) = time632(Ri, P.xsp);
    Li = Cc*(Qr(i)*D.K/(D.tau*s + 1))/(P.tsen*s + 1) ...
         *exp(-(P.Ts/2 + P.avg/2 + theta_mix)*s);
    [~, pmF(i), ~, wcF(i)] = margin(Li);
    fprintf('.');
end
fprintf(' done\n\n');

fig = newFig(figW, 10, 'C8 Sweeps');
tl  = tiledlayout(fig, 1, 2, 'TileSpacing', 'compact', 'Padding', 'compact');

ax = nexttile(tl); hold(ax, 'on');
yyaxis(ax, 'left');
plot(ax, ACH, tauL/60, '-o', 'Color', C.co2, 'LineWidth', 1.6, ...
     'MarkerFaceColor', C.co2, 'MarkerSize', 4);
yline(ax, D.tau/60, ':', 'Color', C.co2, 'LineWidth', 1.1);
ylabel(ax, 'Plant time constant $\tau$ (min)');
ylim(ax, [0 ceil(max(tauL/60)/5)*5]);
ax.YAxis(1).Color = C.co2;
yyaxis(ax, 'right');
plot(ax, ACH, rpL, '-s', 'Color', C.duty, 'LineWidth', 1.6, 'MarkerSize', 4);
yline(ax, 0.3, '--', 'Color', C.bad, 'LineWidth', 1.1);
ylabel(ax, 'Steady ripple (pp)'); ylim(ax, [0 0.45]);
ax.YAxis(2).Color = C.duty;
xlabel(ax, 'Leak (air changes per hour)'); xlim(ax, [1 max(ACH)]);
styleAxes(ax);
panelTitle(ax, '(a)', 'Leak: the plant speeds up, $T_i$ does not');
text(ax, 0.04, 0.90, sprintf('$T_i = %.0f$ min, fixed', D.tau/60), ...
     'Units', 'normalized', 'Color', C.co2, 'FontSize', 8.5);
text(ax, 0.97, 0.70, 'R4 ripple limit', 'Units', 'normalized', ...
     'HorizontalAlignment', 'right', 'Color', C.bad, 'FontSize', 8.5);

ax = nexttile(tl); hold(ax, 'on');
yyaxis(ax, 'left');
plot(ax, Qr*0.1, tbF, '-o', 'Color', C.co2, 'LineWidth', 1.6, ...
     'MarkerFaceColor', C.co2, 'MarkerSize', 4);
ylabel(ax, 'Time to 63.2\% of the step (min)');
ylim(ax, [0 max(5, ceil(max(tbF)/5)*5)]);
ax.YAxis(1).Color = C.co2;
yyaxis(ax, 'right');
plot(ax, Qr*0.1, pmF, '-s', 'Color', C.bad, 'LineWidth', 1.6, 'MarkerSize', 4);
yline(ax, 45, ':', 'Color', C.sp, 'LineWidth', 1.1);
ylabel(ax, sprintf('Phase margin, %d s mixing ($^\\circ$)', theta_mix));
ylim(ax, [0 90]); yticks(ax, 0:15:90);
ax.YAxis(2).Color = C.bad;
xlabel(ax, 'Regulator flow (L/min)'); xlim(ax, [0.05 0.30]);
styleAxes(ax);
panelTitle(ax, '(b)', 'Flow: a faster fill, less margin');
text(ax, 0.97, 0.52, '$45^\circ$', 'Units', 'normalized', ...
     'HorizontalAlignment', 'right', 'Color', C.sp, 'FontSize', 8.5);
saveFig(fig, [pre 'fig_C8_sweeps'], outDir, dpi);

fprintf('LEAK SWEEP   controller fixed at Kp = %.4f, Ti = %g s\n', Kp, D.tau);
fprintf('  %4s %9s %9s %14s %9s %9s\n', 'ACH', 'K (pp)', 'tau (s)', 'To 63.2% (min)', 'Duty', 'Ripple');
for i = 1:numel(ACH)
    fprintf('  %4d %9.1f %9.0f %12.1f %8.2f %% %6.3f pp\n', ...
            ACH(i), KL(i), tauL(i), tbL(i), duL(i), rpL(i));
end
fprintf('\nFLOW SWEEP   %d s mixing delay assumed\n', theta_mix);
fprintf('  %8s %14s %12s %9s\n', 'L/min', 'To 63.2% (min)', 'PM (deg)', 'Ripple');
for i = 1:numel(Qr)
    fprintf('  %8.3f %12.1f %10.0f %8.3f pp\n', Qr(i)*0.1, tbF(i), pmF(i), rpF(i));
end
fprintf('\nLaTeX rows, leak sweep (ACH & K & tau & t63 & ripple):\n');
for i = 1:numel(ACH)
    fprintf('  %d & %.0f & %.0f & %.1f & %.3f \\\\\n', ACH(i), KL(i), tauL(i), tbL(i), rpL(i));
end
fprintf('\nLaTeX rows, flow sweep (L/min & crossover & PM & t63):\n');
for i = 1:numel(Qr)
    fprintf('  %.2f & %.4f & %.0f\\textdegree{} & %.1f \\\\\n', ...
            Qr(i)*0.1, wcF(i), pmF(i), tbF(i));
end
fprintf('------------------------------------------------------------\n\n');

fprintf('All figures saved, prefixed "%s", in:\n  %s\n', pre, outDir);


%% ======================= LOCAL FUNCTIONS =======================
function tb = time632(R, sp)
% Minutes for the 15 s average to cover 63.2 % of the way from its starting
% value to the setpoint. Same definition used for the thermal step tests.
% Measured where the curve is steep, so the pulsing cannot shift it.
    target = R.xm(1) + 0.632*(sp - R.xm(1));
    i = find(R.xm >= target, 1, 'first');
    if isempty(i), tb = NaN; else, tb = R.tmin(i); end
end

function rp = rippleOf(R)
% Peak-to-peak of the true (unaveraged) CO2 over the last 20 minutes.
    last = R.t >= R.t(end) - 20*60;
    rp = max(R.x(last)) - min(R.x(last));
end

function R = simCO2(P, D, O)
% Realistic CO2 loop: nonlinear plant, sensor lag, 0.5 s readings averaged,
% discrete PI every Ts, one time-proportioned valve pulse per window.
    Kp = D.tau/(D.K*P.lam);  Ki = 1/(D.K*P.lam);
    dt = P.dt; N = round(O.Tend/dt) + 1; t = (0:N-1)'*dt;
    QL = P.QL*O.QLx; Qin = P.Qin*O.Qinx;
    x = zeros(N, 1); y = zeros(N, 1); u = zeros(N, 1); v = zeros(N, 1);
    if O.startSS
        x(1) = P.xsp; y(1) = P.xsp;
        mu0 = QL*(P.xsp - P.xa)/(Qin*(P.xs - P.xsp));
        I = mu0/Ki;  reached = true;
    else
        x(1) = P.xa; y(1) = P.xa; I = 0; reached = false;
    end
    nbuf = max(1, round(P.avg/P.tread)); buf = y(1)*ones(nbuf, 1);
    mode = O.mode; uk = 0; openUntil = -1;
    nextRead = 0; nextCtrl = 0;
    if isempty(O.door), kDoor = -1; else, kDoor = round(O.door/dt) + 1; end
    for k = 1:N-1
        tk = t(k);
        if k == kDoor, x(k) = O.doorTo; end
        if tk >= nextRead - 1e-9
            buf = [buf(2:end); y(k)];
            nextRead = nextRead + P.tread;
        end
        if tk >= nextCtrl - 1e-9
            nextCtrl = nextCtrl + P.Ts;
            ym = mean(buf); e = P.xsp - ym;
            if strcmp(mode, 'fullopen')
                if ym >= O.fillTo, mode = 'pi'; else, uk = 1; end
            end
            if strcmp(mode, 'pi')
                if abs(e) < 0.2, reached = true; end
                frz = O.freeze && reached && abs(e) > P.band;
                uraw = Kp*e + Ki*I;
                uk = min(max(uraw, 0), P.umax);
                if ~frz && (uk == uraw || (uraw > P.umax && e < 0) || (uraw < 0 && e > 0))
                    I = I + e*P.Ts;
                end
            end
            ton = round(uk*P.Ts/P.tmin)*P.tmin;
            if ton < P.tmin, ton = 0; end
            openUntil = tk + ton;
        end
        u(k) = uk;
        vk = double(tk < openUntil - 1e-9); v(k) = vk;
        dx = (vk*Qin*(P.xs - x(k)) - QL*(x(k) - P.xa))/P.V;
        x(k+1) = x(k) + dx*dt;
        y(k+1) = y(k) + (x(k) - y(k))/P.tsen*dt;
    end
    u(N) = u(N-1); v(N) = v(N-1);
    xm = movmean(x, round(P.Ts/dt));
    R = struct('t', t, 'tmin', t/60, 'x', x, 'xm', xm, 'y', y, 'u', u, 'v', v);
end

function M = metrics(R, t0, sp)
% Overshoot and ripple. (Timing is handled by time632, see above.)
    m = R.t >= t0; y = R.xm(m);
    M.overshoot = max(0, max(y) - sp);
    last = R.t >= R.t(end) - 20*60;
    M.ripple = max(R.x(last)) - min(R.x(last));
end

function drawTable(ax, hdr, data)
    axis(ax, 'off'); hold(ax, 'on');
    xlim(ax, [0 1]); ylim(ax, [0 1]);
    colX = [0.02 0.48 0.66 0.86]; align = {'left', 'center', 'center', 'center'};
    nR = size(data, 1); rowH = 1/(nR + 1.6);
    yTop = 1 - 0.1*rowH;
    line(ax, [0 1], [yTop yTop], 'Color', 'k', 'LineWidth', 1.0);
    yH = yTop - 0.6*rowH;
    for c = 1:4
        text(ax, colX(c), yH, ['\textbf{' hdr{c} '}'], 'HorizontalAlignment', align{c}, 'FontSize', 9);
    end
    yMid = yTop - 1.2*rowH;
    line(ax, [0 1], [yMid yMid], 'Color', 'k', 'LineWidth', 0.6);
    for r = 1:nR
        yr = yMid - (r - 0.4)*rowH;
        text(ax, colX(1), yr, data{r, 1}, 'HorizontalAlignment', 'left', 'FontSize', 9);
        text(ax, colX(2), yr, sprintf('%.2f', data{r, 2}), 'HorizontalAlignment', 'center', 'FontSize', 9);
        v3 = data{r, 3};
        if ~ischar(v3), v3 = sprintf('%.1f', v3); end
        text(ax, colX(3), yr, v3, 'HorizontalAlignment', 'center', 'FontSize', 9);
        text(ax, colX(4), yr, sprintf('%.3f', data{r, 4}), 'HorizontalAlignment', 'center', 'FontSize', 9);
    end
    yBot = yMid - (nR + 0.1)*rowH;
    line(ax, [0 1], [yBot yBot], 'Color', 'k', 'LineWidth', 1.0);
end

function lgd = mkLegend(ax, labels, loc, handles)
    if nargin < 4
        lgd = legend(ax, labels, 'Location', loc);
    else
        lgd = legend(ax, handles, labels, 'Location', loc);
    end
    set(lgd, 'Box', 'off', 'FontSize', 9, 'Interpreter', 'latex', 'ItemTokenSize', [18 18]);
end

function panelTitle(ax, lab, txt)
    title(ax, ['\textbf{' lab '}$\;$' txt], 'Interpreter', 'latex', ...
          'FontWeight', 'normal', 'FontSize', 10);
    try
        ax.TitleHorizontalAlignment = 'left';
    catch
    end
end

function fig = newFig(w, h, name)
    fig = figure('Name', name, 'Color', 'w', 'Units', 'centimeters', 'Position', [2 2 w h]);
end

function styleAxes(ax)
    set(ax, 'FontSize', 10, 'LineWidth', 0.75, 'Box', 'on', 'TickDir', 'out', ...
        'TickLength', [0.008 0.008], 'Layer', 'top', ...
        'GridAlpha', 0.12, 'MinorGridAlpha', 0.06, 'TickLabelInterpreter', 'latex');
    grid(ax, 'on');
end

function saveFig(fig, name, outDir, dpi)
    drawnow;
    exportgraphics(fig, fullfile(outDir, [name '.png']), ...
                   'Resolution', dpi, 'BackgroundColor', 'white');
end