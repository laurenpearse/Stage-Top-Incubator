%% Incubator humidity and long run: report figures
% Generates the humidity figures for Chapter 8 from the serial log of the
% long run. Needs longrun.csv (see parse_longrun.m).
%
% Styling matches the CO2 and thermal figures: LaTeX interpreter, 16 cm
% wide, 600 dpi PNG, (a) (b) (c) panel titles.
%
% Figure H1 is 16 x 11 cm so it fits on a page with text. A taller figure
% takes more than 70 % of the text height and LaTeX pushes it onto a float
% page of its own, which is what leaves a page half empty.

clear; close all; clc;

%% ======================= SETTINGS =======================
figW   = 16;             % figure width in cm, use width=\textwidth in LaTeX
dpi    = 600;
outDir = pwd;   % figures are saved next to this script
CSV    = 'longrun.csv';

set(groot, 'defaultTextInterpreter', 'latex', ...
           'defaultAxesTickLabelInterpreter', 'latex', ...
           'defaultLegendInterpreter', 'latex', ...
           'defaultAxesFontSize', 10, ...
           'defaultLineLineJoin', 'round');

%% ======================= PARAMETERS =======================
SP_CHAM = 39.0;          % chamber setpoint during the run, degC
SP_GLAS = 44.0;          % glass setpoint, degC
RH_REQ  = 90;            % R3 floor, %
WARMUP  = 3600;          % seconds excluded from the steady state statistics

% ---- Colours, matching the thermal figures in Chapters 6 and 7 ----
C.cham  = [0.000 0.447 0.741];   % chamber air: blue
C.glass = [0.850 0.325 0.098];   % lid glass: orange
C.well  = [0.466 0.674 0.188];   % water in the well: green
C.amb   = [0.45 0.45 0.45];      % room: grey
C.rh    = [0.494 0.184 0.556];   % humidity: purple
C.bad   = [0.635 0.078 0.184];   % requirement limits: dark red

pct = '\%';
if ~exist(outDir, 'dir'), mkdir(outDir); end

%% ======================= DATA =======================
d  = readtable(CSV);
s1 = d(d.segment == 1 & strcmp(d.mode, 'PID'), :);     % the 6.5 h run
h  = s1.t_s/3600;

dew   = dewpoint(s1.T_dht, s1.RH_pct);                       % chamber dew point
rhCh  = s1.RH_pct .* psat(s1.T_dht) ./ psat(s1.T_cham);      % RH at the air temperature
rhCap = 100 * psat(s1.T_E8) ./ psat(s1.T_cham);              % ceiling set by the water

%% ======================= STEADY STATE NUMBERS =======================
k = s1.t_s >= WARMUP;
fprintf('Long run segment 1: %.2f h, steady window %.2f h (%d samples)\n\n', ...
        max(h), (max(s1.t_s(k)) - min(s1.t_s(k)))/3600, sum(k));
rep = @(n, v) fprintf('  %-12s %7.3f  sd %5.3f   [%6.2f  %6.2f]\n', ...
                      n, mean(v), std(v), min(v), max(v));
rep('T_cham',   s1.T_cham(k));
rep('T_glass',  s1.T_glass(k));
rep('T_E8',     s1.T_E8(k));
rep('T_amb',    s1.T_amb(k));
rep('RH',       s1.RH_pct(k));
rep('dew point',dew(k));
rep('heater %', s1.heat_pct(k));
rep('glass %',  s1.glass_pct(k));
fprintf('\n  well minus dew point   %+.3f C\n', mean(s1.T_E8(k)    - dew(k)));
fprintf('  air minus well         %+.3f C\n',   mean(s1.T_cham(k)  - s1.T_E8(k)));
fprintf('  glass minus dew point  %+.3f C\n',   mean(s1.T_glass(k) - dew(k)));
fprintf('  glass minus air        %+.3f C\n',   mean(s1.T_glass(k) - s1.T_cham(k)));
fprintf('  RH at the air temperature  %.1f %%\n', mean(rhCh(k)));
fprintf('  predicted ceiling          %.1f %%\n\n', mean(rhCap(k)));
fprintf('  LaTeX table rows:\n');
fprintf('  Chamber air (\\textdegree{}C) & %.3f & %.3f \\\\\n',  mean(s1.T_cham(k)),  std(s1.T_cham(k)));
fprintf('  Lid glass (\\textdegree{}C) & %.3f & %.3f \\\\\n',    mean(s1.T_glass(k)), std(s1.T_glass(k)));
fprintf('  Water in well E8 (\\textdegree{}C) & %.3f & %.3f \\\\\n', mean(s1.T_E8(k)), std(s1.T_E8(k)));
fprintf('  Room (\\textdegree{}C) & %.3f & %.3f \\\\\n',         mean(s1.T_amb(k)),   std(s1.T_amb(k)));
fprintf('  Relative humidity (\\%%) & %.2f & %.2f \\\\\n',       mean(s1.RH_pct(k)),  std(s1.RH_pct(k)));
fprintf('  Dew point (\\textdegree{}C) & %.2f & %.2f \\\\\n\n',  mean(dew(k)),        std(dew(k)));

%% ======================= FIG H1: THE LONG RUN =======================
%  (a) spans the top row, (b) and (c) share the bottom row, so the figure
%  is wide and short and sits on a page with text.
fig = newFig(figW, 12, 'H1 Long run');
tl  = tiledlayout(fig, 2, 1, 'TileSpacing', 'compact', 'Padding', 'compact');

ax1 = nexttile(tl); hold(ax1, 'on');
p1 = plot(ax1, h, s1.T_glass, 'Color', C.glass, 'LineWidth', 1.3);
p2 = plot(ax1, h, s1.T_cham,  'Color', C.cham,  'LineWidth', 1.3);
p3 = plot(ax1, h, s1.T_E8,    'Color', C.well,  'LineWidth', 1.3);
p4 = plot(ax1, h, s1.T_amb,   'Color', C.amb,   'LineWidth', 1.0);
yline(ax1, SP_GLAS, ':', 'Color', C.glass, 'LineWidth', 1.0);
yline(ax1, SP_CHAM, ':', 'Color', C.cham,  'LineWidth', 1.0);
ylabel(ax1, 'Temperature ($^\circ$C)');
xlim(ax1, [0 max(h)]); ylim(ax1, [17 49]); yticks(ax1, 20:5:45);
lg = mkLegend(ax1, {'Lid glass', 'Chamber air', 'Water in well E8', 'Room'}, ...
              'east', [p1 p2 p3 p4]);
set(lg, 'NumColumns', 2);
lg.Position(2) = lg.Position(2) - 0.06;     % nudge it down
styleAxes(ax1); set(ax1, 'XTickLabel', []);
panelTitle(ax1, '(a)', 'Temperatures over the run (dotted lines: setpoints)');

ax2 = nexttile(tl); hold(ax2, 'on');
p1 = plot(ax2, h, s1.RH_pct, 'Color', C.rh,   'LineWidth', 1.5);
p2 = plot(ax2, h, rhCap, '--', 'Color', C.well, 'LineWidth', 1.2);
yline(ax2, RH_REQ, '-', 'Color', C.bad, 'LineWidth', 1.1);
text(ax2, 0.96, 0.93, ['R3 minimum, 90' pct], 'Units', 'normalized', ...
     'HorizontalAlignment', 'right', 'Color', C.bad, 'FontSize', 8.5);
ylabel(ax2, ['Relative humidity (' pct ')']); xlabel(ax2, 'Run time (h)');
xlim(ax2, [0 max(h)]); ylim(ax2, [60 100]);
mkLegend(ax2, {'Measured', 'Maximum possible'}, 'southeast', [p1 p2]);
styleAxes(ax2);
panelTitle(ax2, '(b)', 'Humidity against R3');

saveFig(fig, 'fig_H1_long_run', outDir, dpi);

%% ======================= FIG H2: RH AGAINST AIR TEMPERATURE =======================
%  Every segment, including the cool-down while the heaters were off after
%  the controller reset. Grey lines are constant dew point.
fig = newFig(figW, 9.5, 'H2 Dew point');
ax  = axes(fig); hold(ax, 'on'); %#ok<LAXES>

Tg = linspace(28, 43, 400);
for Td = 28:2:38
    plot(ax, Tg, 100*psat(Td)./psat(Tg), '-', 'Color', [0.82 0.82 0.82], 'LineWidth', 0.8);
    text(ax, Td + 0.12, 99.4, sprintf('%d', Td), 'FontSize', 8, ...
         'Color', [0.55 0.55 0.55], 'HorizontalAlignment', 'left', ...
         'VerticalAlignment', 'top');
end
text(ax, 0.015, 0.965, 'Grey lines: constant dew point ($^\circ$C)', ...
     'Units', 'normalized', 'Color', [0.55 0.55 0.55], 'FontSize', 8.5);

hs = gobjects(1, 2);
m1 = d.segment == 1;
m2 = d.segment >= 2;
hs(1) = scatter(ax, d.T_dht(m1), d.RH_pct(m1), 7, 'o', 'MarkerEdgeColor', C.rh, ...
                'MarkerEdgeAlpha', 0.30);
hs(2) = scatter(ax, d.T_dht(m2), d.RH_pct(m2), 12, '^', 'MarkerEdgeColor', C.bad, ...
                'MarkerEdgeAlpha', 0.55, 'LineWidth', 0.8);
yline(ax, RH_REQ, '-', 'Color', C.bad, 'LineWidth', 1.1);
text(ax, 28.3, RH_REQ + 1.1, ['R3 minimum, 90' pct], 'Color', C.bad, 'FontSize', 9);

xlabel(ax, 'Air temperature at the humidity sensor ($^\circ$C)');
ylabel(ax, ['Relative humidity (' pct ')']);
xlim(ax, [28 43]); ylim(ax, [70 101]); yticks(ax, 70:5:100);
mkLegend(ax, {'Controlled run, including warm-up', ...
              'Cool-down after the controller reset'}, 'southwest', hs);
styleAxes(ax);
panelTitle(ax, '', 'Humidity rises as the air cools, with the same water in the wells');

saveFig(fig, 'fig_H2_dewpoint', outDir, dpi);

%% ======================= FIG H3: THE RESTART =======================
%  The twenty minutes after the controller reset. The heaters bring the air
%  back up while the water in the wells stays where it is, so the humidity
%  falls. The same mechanism as the main run, run backwards.
s2  = d(d.segment == 2, :);
m2  = s2.t_s/60;

fprintf('\nRestart (segment 2), %.1f min:\n', max(m2));
fprintf('  start  air %.2f C, water %.2f C, RH %.1f %%\n', ...
        s2.T_cham(1), s2.T_E8(1), s2.RH_pct(1));
fprintf('  end    air %.2f C, water %.2f C, RH %.1f %%\n\n', ...
        s2.T_cham(end), s2.T_E8(end), s2.RH_pct(end));

fig = newFig(figW, 9, 'H3 Restart');
tl  = tiledlayout(fig, 2, 1, 'TileSpacing', 'compact', 'Padding', 'compact');

ax1 = nexttile(tl); hold(ax1, 'on');
p1 = plot(ax1, m2, s2.T_cham, 'Color', C.cham, 'LineWidth', 1.5);
p2 = plot(ax1, m2, s2.T_E8,   'Color', C.well, 'LineWidth', 1.5);
ylabel(ax1, 'Temperature ($^\circ$C)');
xlim(ax1, [0 max(m2)]); ylim(ax1, [30 40]);
mkLegend(ax1, {'Chamber air', 'Water in well E8'}, 'southeast', [p1 p2]);
styleAxes(ax1); set(ax1, 'XTickLabel', []);
panelTitle(ax1, '(a)', 'The air warms back up, the water does not');

ax2 = nexttile(tl); hold(ax2, 'on');
plot(ax2, m2, s2.RH_pct, 'Color', C.rh, 'LineWidth', 1.6);
yline(ax2, RH_REQ, '-', 'Color', C.bad, 'LineWidth', 1.1);
text(ax2, 0.96, 0.90, ['R3 minimum, 90' pct], 'Units', 'normalized', ...
     'HorizontalAlignment', 'right', 'Color', C.bad, 'FontSize', 8.5);
text(ax2, m2(1) + 0.3, s2.RH_pct(1), ...
     sprintf('%.1f%s at %.1f$^\\circ$C', s2.RH_pct(1), pct, s2.T_cham(1)), ...
     'FontSize', 8.5, 'VerticalAlignment', 'top');
text(ax2, m2(end) - 0.3, s2.RH_pct(end), ...
     sprintf('%.1f%s at %.1f$^\\circ$C', s2.RH_pct(end), pct, s2.T_cham(end)), ...
     'FontSize', 8.5, 'HorizontalAlignment', 'right', 'VerticalAlignment', 'bottom');
ylabel(ax2, ['Relative humidity (' pct ')']);
xlabel(ax2, 'Time since the controller restarted (min)');
xlim(ax2, [0 max(m2)]); ylim(ax2, [74 94]);
styleAxes(ax2);
panelTitle(ax2, '(b)', 'Humidity falls as the air warms');

saveFig(fig, 'fig_H3_restart', outDir, dpi);

fprintf('Figures saved in:\n  %s\n', outDir);

%% ======================= LOCAL FUNCTIONS =======================
function p = psat(T)
% Saturation vapour pressure over water, hPa.
% Magnus form with the Alduchov and Eskridge (1996) coefficients.
    p = 6.1094 * exp(17.625*T ./ (243.04 + T));
end

function Td = dewpoint(T, RH)
% Dew point from air temperature and relative humidity, degC.
% Inverts psat above, so both must use the same coefficients.
    a  = log(RH/100 .* psat(T) / 6.1094);
    Td = 243.04 * a ./ (17.625 - a);
end

function lgd = mkLegend(ax, labels, loc, handles)
    if nargin < 4
        lgd = legend(ax, labels, 'Location', loc);
    else
        lgd = legend(ax, handles, labels, 'Location', loc);
    end
    set(lgd, 'Box', 'off', 'FontSize', 8.5, 'Interpreter', 'latex', ...
        'ItemTokenSize', [16 16]);
end

function panelTitle(ax, lab, txt)
    if isempty(lab)
        title(ax, txt, 'Interpreter', 'latex', 'FontWeight', 'normal', 'FontSize', 10);
    else
        title(ax, ['\textbf{' lab '}$\;$' txt], 'Interpreter', 'latex', ...
              'FontWeight', 'normal', 'FontSize', 9.5);
    end
    try
        ax.TitleHorizontalAlignment = 'left';
    catch
    end
end

function fig = newFig(w, h, name)
    fig = figure('Name', name, 'Color', 'w', 'Units', 'centimeters', 'Position', [2 2 w h]);
end

function styleAxes(ax)
    set(ax, 'FontSize', 9.5, 'LineWidth', 0.75, 'Box', 'on', 'TickDir', 'out', ...
        'TickLength', [0.008 0.008], 'Layer', 'top', ...
        'GridAlpha', 0.12, 'MinorGridAlpha', 0.06, ...
        'TickLabelInterpreter', 'latex');
    grid(ax, 'on');
end

function saveFig(fig, name, outDir, dpi)
    drawnow;
    exportgraphics(fig, fullfile(outDir, [name '.png']), ...
                   'Resolution', dpi, 'BackgroundColor', 'white');
end