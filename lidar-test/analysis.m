D = [178 310 490 515 820 960];
X_1 = [-10,-7,-1.15,-6.37,-4.75,-4.88];
X_2 = [-6.4,-4.33,1.1,-4.76,-3.44,-3.81];
Y_1 = [7.9,3.56,3.55,4.26,4.78,3.11];
Y_2 = [12.14,6.8,6.33,6.54,6.65,4.78];
Y = 6.25 * 2 ./ (Y_2 - Y_1);
X = 9.25 * 2 ./ (X_2 - X_1);

P_X = polyfit(D,X,1);
F_X = polyval(P_X,D);
P_Y = polyfit(D,Y,1);
F_Y = polyval(P_Y,D);

plot(D,X,'ro',D,Y,'bo');
xlabel('Distance (cm)');
ylabel('Slope (mm/V)');
title('slope in mm/V vs distance');

legend('X (vertical)','Y (horizontal)');
axis([0 1000 0 18])
grid on

hold on
plot(D,F_X,'--r',D,F_Y,'--b');
