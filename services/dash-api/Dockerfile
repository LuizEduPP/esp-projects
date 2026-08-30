FROM node:22-alpine

WORKDIR /app

COPY package.json ./
RUN yarn install --production=false

COPY tsconfig.json ./
COPY src ./src

EXPOSE 8090
CMD ["yarn", "start"]
