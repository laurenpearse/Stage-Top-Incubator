%% Incubator temperature control: report figures (chamber + glass)
% Generates every temperature-control figure for the report from
% simulations of the coupled chamber and glass model. The model and
% controllers match the Simulink model "GlassAndChamber":
%   - chamber and glass plants in first-order (normalised) form
%   - PI controllers with lambda tuning and setpoint weighting b
%   - duty saturation 0..1 and integrator output limited to 0..1
%   - ambient temperature latched at start-up in the controllers
%
% All text uses the LaTeX interpreter so the figures match a LaTeX report.
% Figures are saved as high-resolution PNG files in outDir (set below).
% Requires MATLAB R2020b or newer and the Control System Toolbox.

clear; close all; clc;

%% ======================= SETTINGS =======================
figW   = 16;             % figure width in cm (matches a single-column page)
dpi    = 600;            % PNG resolution
outDir = fullfile(pwd, 'figures');

% LaTeX text everywhere, consistent sizes
set(groot, 'defaultTextInterpreter', 'latex', ...
           'defaultAxesTickLabelInterpreter', 'latex', ...
           'defaultLegendInterpreter', 'latex', ...
           'defaultAxesFontSize', 10, ...
           'defaultLineLineJoin', 'round');

%% ======================= PARAMETERS =======================
% ---- Plant (what the simulation uses) ----
P.Ta    = 20;            % ambient temperature, degC
P.T0    = 20;            % initial chamber and glass temperature, degC
P.Kc    = 128;           % chamber gain, K per unit duty
P.tauc  = 594;           % chamber time constant, s
P.ac_g  = 0.299;         % chamber: glass coupling
P.ac_a  = 0.701;         % chamber: ambient coupling
P.Kg    = 73;            % glass gain, K per unit duty (16 W ITO)
P.taug  = 158.3;         % glass time constant, s
P.ag_c  = 0.383;         % glass: chamber coupling
P.ag_a  = 0.616;         % glass: ambient coupling

% ---- Controller settings ----
P.Tc_sp = 37;  P.Tg_sp = 39;    % setpoints, degC
P.lamc  = 300; P.lamg  = 60;    % lambda (closed-loop time constants), s
P.bc    = 0.5; P.bg    = 0.5;   % setpoint weights

% ---- Simulation settings ----
P.dt         = 0.05;            % integration step, s
P.Tend       = 90*60;           % run length, s
P.glassFixed = false;           % true: glass held at its setpoint
P.delay      = 0;               % transport delay on both duties, s
P.TaFun      = @(t) P.Ta + 0*t; % ambient seen by the PLANT (controllers
                                % always use the latched value P.Ta)

% ---- Design model (what the controllers were tuned on; never changed) ----
D.Kc = 128; D.tauc = 594; D.Kg = 73; D.taug = 158.3;

% ---- Colours (used consistently in every figure) ----
C.ch   = [0.000 0.447 0.741];   % chamber: blue
C.gl   = [0.850 0.325 0.098];   % glass: orange
C.sp   = [0.45 0.45 0.45];      % setpoints: grey
C.amb  = [0.466 0.674 0.188];   % ambient: green
C.warn = [0.635 0.078 0.184];   % limits and critical lines: dark red
C.seq  = [0.45 0.70 0.95; 0.000 0.447 0.741; 0.05 0.20 0.45];   % light to dark blues
C.seqg = [0.98 0.70 0.45; 0.850 0.325 0.098; 0.50 0.15 0.02];   % light to dark oranges

degC = '$^\circ$C';             % degree symbol for labels

if ~exist(outDir, 'dir'), mkdir(outDir); end

% Controller gains, printed for reference
Kp_c = D.tauc/(D.Kc*P.lamc);  Ki_c = 1/(D.Kc*P.lamc);
Kp_g = D.taug/(D.Kg*P.lamg);  Ki_g = 1/(D.Kg*P.lamg);
fprintf('Chamber PI: Kp = %.4f, Ki = %.3e\n', Kp_c, Ki_c);
fprintf('Glass PI:   Kp = %.4f, Ki = %.3e\n\n', Kp_g, Ki_g);

%% ======================= FIG T1: SIMULINK DIAGRAM =======================
try
    load_system('GlassAndChamber');
    print('-sGlassAndChamber', '-dpng', sprintf('-r%d', dpi), fullfile(outDir, 'fig_T1_simulink.png'));
    fprintf('Fig T1: Simulink diagram exported.\n');
catch
    fprintf('Fig T1: model GlassAndChamber not found on the path, skipped.\n');
end

%% ======================= FIG T2: COUPLED WARM-UP =======================
R  = simInc(P, D);
Mc = metrics(R.tmin, R.Tc, P.Tc_sp, 0.2);
Mg = metrics(R.tmin, R.Tg, P.Tg_sp, 0.2);
gap = R.Tg - R.Tc;
fprintf('Fig T2 coupled warm-up:\n');
fprintf('  Chamber: overshoot %.2f degC, settles (+/-0.2) in %.1f min, final duty %.3f\n', ...
        Mc.overshoot, Mc.ts, R.uc(end));
fprintf('  Glass:   overshoot %.2f degC, settles (+/-0.2) in %.1f min, final duty %.3f\n', ...
        Mg.overshoot, Mg.ts, R.ug(end));
fprintf('  Smallest glass-to-chamber margin (after 1 min): %.2f degC\n\n', min(gap(R.tmin > 1)));

fig = newFig(figW, 16, 'T2 Coupled warm-up');
tl = tiledlayout(fig, 4, 1, 'TileSpacing', 'compact', 'Padding', 'compact');

ax1 = nexttile(tl, [2 1]); hold(ax1, 'on');
plot(ax1, R.tmin, R.Tg, 'Color', C.gl, 'LineWidth', 1.8);
plot(ax1, R.tmin, R.Tc, 'Color', C.ch, 'LineWidth', 1.8);
yline(ax1, P.Tg_sp, '--', 'Color', C.gl, 'LineWidth', 0.9);
yline(ax1, P.Tc_sp, '--', 'Color', C.ch, 'LineWidth', 0.9);
ylabel(ax1, ['Temperature (' degC ')']);
mkLegend(ax1, {'Glass $T_g$', 'Chamber $T_c$', ['Glass setpoint (39 ' degC ')'], ...
               ['Chamber setpoint (37 ' degC ')']}, 'southeast');
ylim(ax1, [P.T0 - 1, 41]);
styleAxes(ax1); panelTitle(ax1, '(a)', 'Chamber and glass temperatures');

ax2 = nexttile(tl); hold(ax2, 'on');
plot(ax2, R.tmin, gap, 'Color', [0.15 0.15 0.15], 'LineWidth', 1.6);
yline(ax2, 0, '-', 'Color', C.warn, 'LineWidth', 1.1);
ylabel(ax2, ['$T_g - T_c$ (' degC ')']);
ylim(ax2, [-1.5, ceil(max(gap)) + 1.5]);
text(ax2, 0.985, 0.08, 'Condensation limit, $T_g = T_c$', 'Units', 'normalized', ...
     'HorizontalAlignment', 'right', 'VerticalAlignment', 'bottom', ...
     'Color', C.warn, 'FontSize', 9);
styleAxes(ax2); panelTitle(ax2, '(b)', 'Condensation margin');

ax3 = nexttile(tl); hold(ax3, 'on');
plot(ax3, R.tmin, 100*R.ug, 'Color', C.gl, 'LineWidth', 1.6);
plot(ax3, R.tmin, 100*R.uc, 'Color', C.ch, 'LineWidth', 1.6);
ylabel(ax3, 'Duty cycle (\%)'); xlabel(ax3, 'Time (min)');
ylim(ax3, [0, 10*ceil(max(100*[R.ug; R.uc])/10)]);
mkLegend(ax3, {'ITO glass heater $u_g$', 'Film heaters $u_c$'}, 'northeast');
styleAxes(ax3); panelTitle(ax3, '(c)', 'Heater duty cycles');

linkaxes([ax1 ax2 ax3], 'x'); xlim(ax1, [0 R.tmin(end)]);
set([ax1 ax2], 'XTickLabel', []);
saveFig(fig, 'fig_T2_coupled_warmup', outDir, dpi);

%% ======================= FIG T4: SETPOINT WEIGHTING =======================
Q = P; Q.bc = 1;   Rc1  = simInc(Q, D);
Q = P; Q.bc = 0.5; Rc05 = simInc(Q, D);
Q = P; Q.bg = 1;   Rg1  = simInc(Q, D);
Q = P; Q.bg = 0.5; Rg05 = simInc(Q, D);
Mc1 = metrics(Rc1.tmin, Rc1.Tc, P.Tc_sp, 0.2);  Mc05 = metrics(Rc05.tmin, Rc05.Tc, P.Tc_sp, 0.2);
Mg1 = metrics(Rg1.tmin, Rg1.Tg, P.Tg_sp, 0.2);  Mg05 = metrics(Rg05.tmin, Rg05.Tg, P.Tg_sp, 0.2);
fprintf('Fig T4 setpoint weighting:\n');
fprintf('  Chamber b=1:   overshoot %.2f degC, settles %.1f min\n', Mc1.overshoot, Mc1.ts);
fprintf('  Chamber b=0.5: overshoot %.2f degC, settles %.1f min\n', Mc05.overshoot, Mc05.ts);
fprintf('  Glass   b=1:   overshoot %.2f degC, settles %.1f min\n', Mg1.overshoot, Mg1.ts);
fprintf('  Glass   b=0.5: overshoot %.2f degC, settles %.1f min\n\n', Mg05.overshoot, Mg05.ts);

lc = {sprintf('$b = 1$, overshoot %.2f $^\\circ$C', Mc1.overshoot), ...
      sprintf('$b = 0.5$, overshoot %.2f $^\\circ$C', Mc05.overshoot)};
lg = {sprintf('$b = 1$, overshoot %.2f $^\\circ$C', Mg1.overshoot), ...
      sprintf('$b = 0.5$, overshoot %.2f $^\\circ$C', Mg05.overshoot)};

fig = newFig(figW, 13, 'T4 Setpoint weighting');
tl = tiledlayout(fig, 2, 2, 'TileSpacing', 'compact', 'Padding', 'compact');

ax = nexttile(tl); hold(ax, 'on');
h1 = plot(ax, Rc1.tmin, Rc1.Tc, ':', 'Color', C.ch, 'LineWidth', 1.8);
h2 = plot(ax, Rc05.tmin, Rc05.Tc, '-', 'Color', C.ch, 'LineWidth', 1.8);
yline(ax, P.Tc_sp, '--', 'Color', C.sp);
ylabel(ax, ['$T_c$ (' degC ')']); xlim(ax, [0 90]); ylim(ax, [P.T0 - 1, 40]);
mkLegend(ax, lc, 'southeast', [h1 h2]);
styleAxes(ax); panelTitle(ax, '(a)', 'Chamber warm-up');

ax = nexttile(tl); hold(ax, 'on');
h1 = plot(ax, Rg1.tmin, Rg1.Tg, ':', 'Color', C.gl, 'LineWidth', 1.8);
h2 = plot(ax, Rg05.tmin, Rg05.Tg, '-', 'Color', C.gl, 'LineWidth', 1.8);
yline(ax, P.Tg_sp, '--', 'Color', C.sp);
ylabel(ax, ['$T_g$ (' degC ')']); xlim(ax, [0 40]); ylim(ax, [P.T0 - 1, 42]);
mkLegend(ax, lg, 'southeast', [h1 h2]);
styleAxes(ax); panelTitle(ax, '(b)', 'Glass warm-up');

ax = nexttile(tl); hold(ax, 'on');
hb = fill(ax, [0 90 90 0], P.Tc_sp + [-0.2 -0.2 0.2 0.2], C.sp, 'FaceAlpha', 0.12, 'EdgeColor', 'none');
plot(ax, Rc1.tmin, Rc1.Tc, ':', 'Color', C.ch, 'LineWidth', 1.8);
plot(ax, Rc05.tmin, Rc05.Tc, '-', 'Color', C.ch, 'LineWidth', 1.8);
yline(ax, P.Tc_sp, '--', 'Color', C.sp);
ylim(ax, [P.Tc_sp - 1, P.Tc_sp + 0.6]); xlim(ax, [0 90]);
ylabel(ax, ['$T_c$ (' degC ')']); xlabel(ax, 'Time (min)');
mkLegend(ax, {['$\pm$0.2 ' degC ' band']}, 'southeast', hb);
styleAxes(ax); panelTitle(ax, '(c)', 'Chamber, close to setpoint');

ax = nexttile(tl); hold(ax, 'on');
hb = fill(ax, [0 40 40 0], P.Tg_sp + [-0.2 -0.2 0.2 0.2], C.sp, 'FaceAlpha', 0.12, 'EdgeColor', 'none');
plot(ax, Rg1.tmin, Rg1.Tg, ':', 'Color', C.gl, 'LineWidth', 1.8);
plot(ax, Rg05.tmin, Rg05.Tg, '-', 'Color', C.gl, 'LineWidth', 1.8);
yline(ax, P.Tg_sp, '--', 'Color', C.sp);
ylim(ax, [P.Tg_sp - 1, P.Tg_sp + 0.6]); xlim(ax, [0 40]);
ylabel(ax, ['$T_g$ (' degC ')']); xlabel(ax, 'Time (min)');
mkLegend(ax, {['$\pm$0.2 ' degC ' band']}, 'southeast', hb);
styleAxes(ax); panelTitle(ax, '(d)', 'Glass, close to setpoint');
saveFig(fig, 'fig_T4_setpoint_weighting', outDir, dpi);

%% ======================= FIG T5: AMBIENT DISTURBANCES =======================
td  = 150*60;                 % disturbance starts after full warm-up, s
dTa = -4;                     % ambient change, degC (20 -> 16)
Q = P; Q.Tend = td + 150*60;
Q.TaFun = @(t) P.Ta + dTa*(t >= td);
Rs = simInc(Q, D);                                          % step
Q.TaFun = @(t) P.Ta + dTa*min(max((t - td)/7200, 0), 1);    % ramp over 2 h
Rr = simInc(Q, D);

win = Rs.t >= td - 10*60;
tS  = (Rs.t(win) - td)/60;
fprintf('Fig T5 ambient disturbances (20 -> 16 degC):\n');
fprintf('  Step: chamber dip %.2f degC, glass dip %.2f degC\n', ...
        min(Rs.Tc(win)) - P.Tc_sp, min(Rs.Tg(win)) - P.Tg_sp);
fprintf('  Ramp: chamber dip %.2f degC, glass dip %.2f degC\n', ...
        min(Rr.Tc(win)) - P.Tc_sp, min(Rr.Tg(win)) - P.Tg_sp);
fprintf('  Smallest glass-to-chamber margin: step %.2f, ramp %.2f degC\n\n', ...
        min(Rs.Tg(win) - Rs.Tc(win)), min(Rr.Tg(win) - Rr.Tc(win)));

fig = newFig(figW, 13, 'T5 Ambient disturbances');
tl = tiledlayout(fig, 3, 2, 'TileSpacing', 'compact', 'Padding', 'compact');
cases = {Rs, Rr};
names = {['step, 20 $\rightarrow$ 16 ' degC], ['ramp, 20 $\rightarrow$ 16 ' degC ' over 2 h']};
labs  = {'(a)', '(b)'; '(c)', '(d)'};
axs = gobjects(2, 2);
for j = 1:2
    Rj = cases{j};
    ax = nexttile(tl, j); hold(ax, 'on');
    plot(ax, tS, Rj.Ta(win), 'Color', C.amb, 'LineWidth', 1.8);
    ylabel(ax, ['$T_a$ (' degC ')']); ylim(ax, [15 21]);
    styleAxes(ax); panelTitle(ax, labs{1, j}, ['Ambient ' names{j}]); axs(1, j) = ax;

    ax = nexttile(tl, j + 2, [2 1]); hold(ax, 'on');
    h1 = plot(ax, tS, Rj.Tg(win) - P.Tg_sp, 'Color', C.gl, 'LineWidth', 1.8);
    h2 = plot(ax, tS, Rj.Tc(win) - P.Tc_sp, 'Color', C.ch, 'LineWidth', 1.8);
    yline(ax, 0, '--', 'Color', C.sp);
    ylabel(ax, ['Deviation from setpoint (' degC ')']);
    xlabel(ax, 'Time since disturbance starts (min)');
    mkLegend(ax, {sprintf('Glass, largest dip %.2f $^\\circ$C', min(Rj.Tg(win)) - P.Tg_sp), ...
                  sprintf('Chamber, largest dip %.2f $^\\circ$C', min(Rj.Tc(win)) - P.Tc_sp)}, ...
             'southeast', [h1 h2]);
    styleAxes(ax); panelTitle(ax, labs{2, j}, 'Temperature response'); axs(2, j) = ax;
end
linkaxes(axs(:), 'x'); xlim(axs(1), [tS(1) tS(end)]);
linkaxes(axs(2, :), 'y');
set(axs(1, :), 'XTickLabel', []);
saveFig(fig, 'fig_T5_ambient_disturbances', outDir, dpi);

%% ======================= FIG T6: ROBUSTNESS =======================
Cvals   = [110 167 250];                 % chamber capacitance, J/K
tauVals = D.tauc*Cvals/167;
Pvals   = [25 36 47];                    % film heater power, W (+/-30%)
Kvals   = D.Kc*Pvals/36;

fig = newFig(figW, 13, 'T6 Robustness');
tl = tiledlayout(fig, 2, 2, 'TileSpacing', 'compact', 'Padding', 'compact');
axA = nexttile(tl, 1); hold(axA, 'on');
axB = nexttile(tl, 2); hold(axB, 'on');
axC = nexttile(tl, 3); hold(axC, 'on');
axD = nexttile(tl, 4); hold(axD, 'on');
for ax = [axC axD]
    fill(ax, [0 90 90 0], P.Tc_sp + [-0.2 -0.2 0.2 0.2], C.sp, 'FaceAlpha', 0.12, 'EdgeColor', 'none');
end
rows = {};
legA = cell(1, 3); legB = cell(1, 3);
hA = gobjects(1, 3); hB = gobjects(1, 3);
for i = 1:3
    lw = 1.3 + 0.6*(i == 2);
    Q = P; Q.tauc = tauVals(i); Ri = simInc(Q, D);
    Mi = metrics(Ri.tmin, Ri.Tc, P.Tc_sp, 0.2);
    hA(i) = plot(axA, Ri.tmin, Ri.Tc, 'Color', C.seq(i, :), 'LineWidth', lw);
    plot(axC, Ri.tmin, Ri.Tc, 'Color', C.seq(i, :), 'LineWidth', lw);
    legA{i} = sprintf('$C_c = %d$ J/K', Cvals(i));
    rows(end+1, :) = {sprintf('C_c = %d J/K', Cvals(i)), Mi.overshoot, Mi.ts}; %#ok<SAGROW>

    Q = P; Q.Kc = Kvals(i); Ri = simInc(Q, D);
    Mi = metrics(Ri.tmin, Ri.Tc, P.Tc_sp, 0.2);
    hB(i) = plot(axB, Ri.tmin, Ri.Tc, 'Color', C.seqg(i, :), 'LineWidth', lw);
    plot(axD, Ri.tmin, Ri.Tc, 'Color', C.seqg(i, :), 'LineWidth', lw);
    legB{i} = sprintf('$P_h = %d$ W', Pvals(i));
    rows(end+1, :) = {sprintf('P_h = %d W', Pvals(i)), Mi.overshoot, Mi.ts}; %#ok<SAGROW>
end
legA{2} = [legA{2} ' (design)'];  legB{2} = [legB{2} ' (design)'];
for ax = [axA axB]
    yline(ax, P.Tc_sp, '--', 'Color', C.sp); xlim(ax, [0 90]); ylim(ax, [P.T0 - 1, 40]);
    ylabel(ax, ['$T_c$ (' degC ')']); styleAxes(ax);
end
for ax = [axC axD]
    yline(ax, P.Tc_sp, '--', 'Color', C.sp);
    xlim(ax, [0 90]); ylim(ax, [P.Tc_sp - 1, P.Tc_sp + 0.6]);
    ylabel(ax, ['$T_c$ (' degC ')']); xlabel(ax, 'Time (min)'); styleAxes(ax);
end
mkLegend(axA, legA, 'southeast', hA); mkLegend(axB, legB, 'southeast', hB);
panelTitle(axA, '(a)', 'Chamber capacitance varied');
panelTitle(axB, '(b)', 'Heater power varied');
panelTitle(axC, '(c)', 'Capacitance, close to setpoint');
panelTitle(axD, '(d)', 'Heater power, close to setpoint');
saveFig(fig, 'fig_T6_robustness', outDir, dpi);

fprintf('Fig T6 robustness (chamber, controller unchanged):\n');
fprintf('  %-16s %12s %14s\n', 'Plant case', 'Overshoot', 'Settling time');
for i = 1:size(rows, 1)
    fprintf('  %-16s %9.2f degC %10.1f min\n', rows{i, 1}, rows{i, 2}, rows{i, 3});
end
fprintf('\n  LaTeX table rows:\n');
for i = 1:size(rows, 1)
    lbl = strrep(strrep(rows{i, 1}, 'C_c', '$C_c$'), 'P_h', '$P_h$');
    fprintf('  %s & %.2f & %.1f \\\\\n', lbl, rows{i, 2}, rows{i, 3});
end
fprintf('\n');

%% ======================= FIG T7: GLASS FIXED VS CONTROLLED =======================
Q = P; Q.glassFixed = true; Rf = simInc(Q, D);
Mf = metrics(Rf.tmin, Rf.Tc, P.Tc_sp, 0.2);
fprintf('Fig T7: chamber with glass fixed: overshoot %.2f, settles %.1f min\n', Mf.overshoot, Mf.ts);
fprintf('        chamber with glass loop:  overshoot %.2f, settles %.1f min\n', Mc.overshoot, Mc.ts);
fprintf('        largest difference between the two: %.2f degC\n\n', max(abs(Rf.Tc - R.Tc)));

fig = newFig(figW, 8.5, 'T7 Glass fixed vs controlled');
tl = tiledlayout(fig, 1, 3, 'TileSpacing', 'compact', 'Padding', 'compact');
ax = nexttile(tl, [1 2]); hold(ax, 'on');
h1 = plot(ax, Rf.tmin, Rf.Tc, '--', 'Color', C.ch, 'LineWidth', 1.8);
h2 = plot(ax, R.tmin, R.Tc, '-', 'Color', C.ch, 'LineWidth', 1.8);
yline(ax, P.Tc_sp, ':', 'Color', C.sp);
xlabel(ax, 'Time (min)'); ylabel(ax, ['Chamber temperature $T_c$ (' degC ')']);
xlim(ax, [0 90]); ylim(ax, [P.T0 - 1, 39]);
mkLegend(ax, {['Glass held at 39 ' degC ' (disturbance model)'], ...
              'Glass under its own PI loop (coupled)'}, 'southeast', [h1 h2]);
styleAxes(ax); panelTitle(ax, '(a)', 'Full warm-up');
ax = nexttile(tl); hold(ax, 'on');
plot(ax, Rf.tmin, Rf.Tc, '--', 'Color', C.ch, 'LineWidth', 1.8);
plot(ax, R.tmin, R.Tc, '-', 'Color', C.ch, 'LineWidth', 1.8);
yline(ax, P.Tc_sp, ':', 'Color', C.sp);
xlim(ax, [20 60]); ylim(ax, [P.Tc_sp - 0.8, P.Tc_sp + 0.2]);
xlabel(ax, 'Time (min)');
styleAxes(ax); panelTitle(ax, '(b)', 'Close to setpoint');
saveFig(fig, 'fig_T7_glass_fixed_vs_coupled', outDir, dpi);

%% ======================= FIG T8: BODE AND NICHOLS =======================
s  = tf('s');
Gc = D.Kc/(D.tauc*s + 1);   Cc = Kp_c + Ki_c/s;   Lc = minreal(Cc*Gc);
Gg = D.Kg/(D.taug*s + 1);   Cg = Kp_g + Ki_g/s;   Lg = minreal(Cg*Gg);
w  = logspace(-5, 0, 800);

[mc, pc] = bode(Lc, w); mc = 20*log10(squeeze(mc)); pc = squeeze(pc);
[mg, pg] = bode(Lg, w); mg = 20*log10(squeeze(mg)); pg = squeeze(pg);
[~, Pmc, ~, Wcc] = margin(Lc);
[~, Pmg, ~, Wcg] = margin(Lg);
fprintf('Fig T8: chamber PM %.1f deg at %.2e rad/s; glass PM %.1f deg at %.2e rad/s\n\n', ...
        Pmc, Wcc, Pmg, Wcg);

fig = newFig(figW, 15, 'T8 Bode and Nichols');
tl = tiledlayout(fig, 2, 2, 'TileSpacing', 'compact', 'Padding', 'compact');

ax = nexttile(tl, 1); hold(ax, 'on');
h1 = plot(ax, w, mc, 'Color', C.ch, 'LineWidth', 1.8);
h2 = plot(ax, w, mg, 'Color', C.gl, 'LineWidth', 1.8);
yline(ax, 0, '-', 'Color', C.sp);
xline(ax, Wcc, ':', 'Color', C.ch, 'LineWidth', 1.1);
xline(ax, Wcg, ':', 'Color', C.gl, 'LineWidth', 1.1);
set(ax, 'XScale', 'log'); xticks(ax, 10.^(-5:0)); xlim(ax, [w(1) w(end)]);
ylabel(ax, 'Magnitude (dB)');
mkLegend(ax, {'Chamber, $L_c(s) = 1/(300s)$', 'Glass, $L_g(s) = 1/(60s)$'}, 'southwest', [h1 h2]);
styleAxes(ax); panelTitle(ax, '(a)', 'Loop gain');

ax = nexttile(tl, 3); hold(ax, 'on');
h1 = plot(ax, w, pc, 'Color', C.ch, 'LineWidth', 2.4);
h2 = plot(ax, w, pg, '--', 'Color', C.gl, 'LineWidth', 1.6);
yline(ax, -180, '-', 'Color', C.warn, 'LineWidth', 1.1);
set(ax, 'XScale', 'log'); xticks(ax, 10.^(-5:0)); xlim(ax, [w(1) w(end)]);
ylim(ax, [-200 0]); yticks(ax, -180:45:0);
xlabel(ax, 'Frequency (rad/s)'); ylabel(ax, 'Phase ($^\circ$)');
mkLegend(ax, {sprintf('Chamber, PM = %.0f$^\\circ$', Pmc), sprintf('Glass, PM = %.0f$^\\circ$', Pmg)}, ...
         'northeast', [h1 h2]);
text(ax, 0.02, 0.14, 'Stability limit, $-180^\circ$', 'Units', 'normalized', ...
     'Color', C.warn, 'FontSize', 9);
styleAxes(ax); panelTitle(ax, '(b)', 'Loop phase');

nicholsPanel(nexttile(tl, 2), Lc, [0 1.5 30 100], w, C.ch, C, '(c)', 'Chamber loop, added delay');
nicholsPanel(nexttile(tl, 4), Lg, [0 1.5 10 30],  w, C.gl, C, '(d)', 'Glass loop, added delay');
saveFig(fig, 'fig_T8_bode_nichols', outDir, dpi);

%% ======================= FIG T9: DISCRETE VS CONTINUOUS =======================
Ts = 1;  z = tf('z', Ts);
Tcc = feedback(Cc*Gc, 1);   Tcz = feedback((Kp_c + Ki_c*Ts/(z - 1))*c2d(Gc, Ts, 'zoh'), 1);
Tgc = feedback(Cg*Gg, 1);   Tgz = feedback((Kp_g + Ki_g*Ts/(z - 1))*c2d(Gg, Ts, 'zoh'), 1);
tc = (0:Ts:2400)';  tg = (0:Ts:600)';
yc  = step(17*Tcc, tc); ycz = step(17*Tcz, tc);
yg  = step(19*Tgc, tg); ygz = step(19*Tgz, tg);
fprintf('Fig T9 discrete pole magnitudes: chamber %s, glass %s\n', ...
        mat2str(sort(abs(pole(Tcz))).', 6), mat2str(sort(abs(pole(Tgz))).', 6));
fprintf('       largest difference: chamber %.3f degC, glass %.3f degC\n', ...
        max(abs(yc - ycz)), max(abs(yg - ygz)));

fig = newFig(figW, 7.5, 'T9 Discrete vs continuous');
tl = tiledlayout(fig, 1, 2, 'TileSpacing', 'compact', 'Padding', 'compact');
ax = nexttile(tl); hold(ax, 'on');
h1 = plot(ax, tc/60, yc, '-', 'Color', C.ch, 'LineWidth', 3.0);
h2 = stairs(ax, tc/60, ycz, '-', 'Color', [0 0 0], 'LineWidth', 0.9);
xlabel(ax, 'Time (min)'); ylabel(ax, ['Rise above ambient (' degC ')']);
ylim(ax, [0 20]);
mkLegend(ax, {'Continuous design', 'Discrete, $T_s = 1$ s'}, 'southeast', [h1 h2]);
styleAxes(ax); panelTitle(ax, '(a)', ['Chamber loop, 17 ' degC ' step']);
ax = nexttile(tl); hold(ax, 'on');
h1 = plot(ax, tg/60, yg, '-', 'Color', C.gl, 'LineWidth', 3.0);
h2 = stairs(ax, tg/60, ygz, '-', 'Color', [0 0 0], 'LineWidth', 0.9);
xlabel(ax, 'Time (min)'); ylim(ax, [0 20]);
mkLegend(ax, {'Continuous design', 'Discrete, $T_s = 1$ s'}, 'southeast', [h1 h2]);
styleAxes(ax); panelTitle(ax, '(b)', ['Glass loop, 19 ' degC ' step']);
saveFig(fig, 'fig_T9_discrete_vs_continuous', outDir, dpi);

fprintf('\nAll figures saved in:\n  %s\n', outDir);


%% ======================= LOCAL FUNCTIONS =======================
function R = simInc(P, D)
% Simulates the coupled chamber and glass loops (forward Euler, step P.dt).
    Kp_c = D.tauc/(D.Kc*P.lamc);  Ki_c = 1/(D.Kc*P.lamc);
    Kp_g = D.taug/(D.Kg*P.lamg);  Ki_g = 1/(D.Kg*P.lamg);
    N  = round(P.Tend/P.dt) + 1;
    t  = (0:N-1)'*P.dt;
    Ta = P.TaFun(t);
    Tc = zeros(N, 1); Tg = zeros(N, 1); uc = zeros(N, 1); ug = zeros(N, 1);
    Tc(1) = P.T0;
    if P.glassFixed, Tg(1) = P.Tg_sp; else, Tg(1) = P.T0; end
    Ic = 0; Ig = 0;                     % integrator outputs, limited to 0..1
    nd = round(P.delay/P.dt);
    for k = 1:N-1
        ec = P.Tc_sp - Tc(k);
        uc(k) = min(max(Kp_c*(P.bc*P.Tc_sp + (1 - P.bc)*P.Ta - Tc(k)) + Ic, 0), 1);
        Ic = min(max(Ic + Ki_c*ec*P.dt, 0), 1);
        eg = P.Tg_sp - Tg(k);
        ug(k) = min(max(Kp_g*(P.bg*P.Tg_sp + (1 - P.bg)*P.Ta - Tg(k)) + Ig, 0), 1);
        Ig = min(max(Ig + Ki_g*eg*P.dt, 0), 1);
        kd = max(k - nd, 1);
        dTc = (P.Kc*uc(kd) + P.ac_g*Tg(k) + P.ac_a*Ta(k) - Tc(k))/P.tauc;
        Tc(k+1) = Tc(k) + dTc*P.dt;
        if P.glassFixed
            Tg(k+1) = P.Tg_sp;
        else
            dTg = (P.Kg*ug(kd) + P.ag_c*Tc(k) + P.ag_a*Ta(k) - Tg(k))/P.taug;
            Tg(k+1) = Tg(k) + dTg*P.dt;
        end
    end
    uc(N) = uc(N-1); ug(N) = ug(N-1);
    R = struct('t', t, 'tmin', t/60, 'Tc', Tc, 'Tg', Tg, 'uc', uc, 'ug', ug, 'Ta', Ta);
end

function M = metrics(tmin, y, sp, tol)
% Overshoot above setpoint and settling time into +/- tol.
    M.overshoot = max(0, max(y) - sp);
    idx = find(abs(y - sp) > tol, 1, 'last');
    if isempty(idx), M.ts = 0; else, M.ts = tmin(min(idx + 1, numel(tmin))); end
end

function nicholsPanel(ax, L, delays, w, mainCol, C, lab, ttl)
% Nichols chart of L(s) with added pure delays.
    hold(ax, 'on');
    s = tf('s');
    greys = linspace(0.72, 0.15, numel(delays) - 1)' * [1 1 1];
    h = gobjects(1, numel(delays) + 1);
    names = cell(1, numel(delays) + 1);
    for i = 1:numel(delays)
        Ld = L*exp(-delays(i)*s);
        [m, p] = bode(Ld, w);
        m = 20*log10(squeeze(m)); p = squeeze(p);
        [~, Pm] = margin(Ld);
        if i == 1, col = mainCol; lw = 2.2; else, col = greys(i - 1, :); lw = 1.4; end
        h(i) = plot(ax, p, m, 'Color', col, 'LineWidth', lw);
        names{i} = sprintf('%g s delay, PM = %.0f$^\\circ$', delays(i), Pm);
    end
    xline(ax, -180, ':', 'Color', C.warn); yline(ax, 0, ':', 'Color', C.sp);
    h(end) = plot(ax, -180, 0, '+', 'Color', C.warn, 'MarkerSize', 11, 'LineWidth', 1.8);
    names{end} = 'Critical point';
    xlim(ax, [-270 -75]); ylim(ax, [-30 40]); xticks(ax, -270:45:-90);
    xlabel(ax, 'Open-loop phase ($^\circ$)'); ylabel(ax, 'Open-loop gain (dB)');
    mkLegend(ax, names, 'northwest', h);
    styleAxes(ax); panelTitle(ax, lab, ttl);
end

function lgd = mkLegend(ax, labels, loc, handles)
% Clean legend: no box, LaTeX text, consistent size.
    if nargin < 4
        lgd = legend(ax, labels, 'Location', loc);
    else
        lgd = legend(ax, handles, labels, 'Location', loc);
    end
    set(lgd, 'Box', 'off', 'FontSize', 9, 'Interpreter', 'latex', 'ItemTokenSize', [18 18]);
end

function panelTitle(ax, lab, txt)
% Left-aligned panel title, e.g. "(a) Chamber warm-up", above the axes.
    title(ax, ['\textbf{' lab '} ' txt], 'Interpreter', 'latex', 'FontWeight', 'normal', ...
          'FontSize', 10);
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
        'GridAlpha', 0.12, 'MinorGridAlpha', 0.06, ...
        'TickLabelInterpreter', 'latex');
    grid(ax, 'on');
end

function saveFig(fig, name, outDir, dpi)
% Saves a tightly cropped, white-background PNG.
    drawnow;
    exportgraphics(fig, fullfile(outDir, [name '.png']), ...
                   'Resolution', dpi, 'BackgroundColor', 'white');
end