start_index = 2;
end_index = 1500;
size = end_index - start_index + 1;
M = csvread('test285.csv',start_index,0,[start_index 0 end_index 3]);

X_1 = M(1:800,1);
X_2 = M(1:800,3);
Y_1 = M(801:size,2);
Y_2 = M(801:size,4);

figure;
plot(X_1,X_2);
title 'X'
figure;
plot(Y_1,Y_2);
title 'Y'


